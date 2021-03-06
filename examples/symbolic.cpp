#include <iostream>
#include <vector>
#include <array>
#include <functional>

//#define VEXCL_SHOW_KERNELS
#include <vexcl/vexcl.hpp>

// http://headmyshoulder.github.com/odeint-v2
#include <boost/numeric/odeint.hpp>

namespace odeint = boost::numeric::odeint;
namespace fusion = boost::fusion;

typedef double value_type;
typedef vex::generator::symbolic< value_type > sym_value;
typedef std::array<sym_value, 3> sym_state;

// System  function for Lorenz attractor ensemble ODE.
// [1] http://headmyshoulder.github.com/odeint-v2/doc/boost_numeric_odeint/tutorial/chaotic_systems_and_lyapunov_exponents.html
// This is only used to record operations chain for autogenerated kernel.
struct sys_func
{
    const value_type sigma;
    const value_type b;
    const sym_value &R;

    sys_func(value_type sigma, value_type b, const sym_value &R)
        : sigma(sigma), b(b), R(R) {}

    void operator()( const sym_state &x , sym_state &dxdt , value_type t ) const
    {
        dxdt[0] = sigma * (x[1] - x[0]);
        dxdt[1] = R * x[0] - x[1] - x[0] * x[2];
        dxdt[2] = x[0] * x[1] - b * x[2];
    }
};

int main( int argc , char **argv )
{
    size_t n;
    const value_type dt = 0.01;
    const value_type t_max = 100.0;

    using namespace std;

    n = argc > 1 ? atoi( argv[1] ) : 1024;

    vex::Context ctx( vex::Filter::Exclusive( vex::Filter::DoublePrecision && vex::Filter::Env ) );
    cout << ctx << endl;

    // Custom kernel body will be recorded here:
    std::ostringstream body;
    vex::generator::set_recorder(body);

    // State types that would become kernel parameters:
    sym_state sym_S = {{
        sym_value::VectorParameter,
        sym_value::VectorParameter,
        sym_value::VectorParameter
    }};

    // Const kernel parameter.
    sym_value sym_R(sym_value::VectorParameter, sym_value::Const);

    /* Odeint is modern C++ library for ODE solution. We can use its collection
     * of ODE steppers to generate effective kernel which would compute one
     * iteration of a 4th order Runge-Kutta method. For that, we instantiate
     * appropriate odeint stepper with vex::symbolic<double> as a value type.
     * One iteration of the stepper would provide us with sequence of
     * expressions suitable for generation of requiered kernel.  This technique
     * may be used with any generic algorithms for generation of customized and
     * effective kernels.
     */
    // Symbolic stepper:
    odeint::runge_kutta4<
            sym_state , value_type , sym_state , value_type ,
            odeint::range_algebra , odeint::default_operations
            > sym_stepper;

    sys_func sys(10.0, 8.0 / 3.0, sym_R);
    sym_stepper.do_step(std::ref(sys), sym_S, 0, dt);

    auto kernel = vex::generator::build_kernel(ctx.queue(), "lorenz", body.str(),
            sym_S[0], sym_S[1], sym_S[2], sym_R
            );

    // Real state initialization:
    value_type Rmin = 0.1 , Rmax = 50.0 , dR = ( Rmax - Rmin ) / value_type( n - 1 );
    std::vector<value_type> r( n );
    for( size_t i=0 ; i<n ; ++i ) r[i] = Rmin + dR * value_type( i );

    vex::vector<value_type> X(ctx.queue(), n);
    vex::vector<value_type> Y(ctx.queue(), n);
    vex::vector<value_type> Z(ctx.queue(), n);
    vex::vector<value_type> R(ctx.queue(), r);

    X = 10.0;
    Y = 10.0;
    Z = 10.0;

    // Integration loop:
    for(value_type t = 0; t < t_max; t += dt)
        kernel(X, Y, Z, R);

    std::vector< value_type > result( n );
    vex::copy( X , result );
    cout << result[0] << endl;
}

// vim: et
