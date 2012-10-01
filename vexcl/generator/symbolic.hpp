#ifndef VEXCL_SYMBOLIC_HPP
#define VEXCL_SYMBOLIC_HPP

#include <iostream>
#include <string>
#include <sstream>
#include <map>
#include <exception>
#include <type_traits>
#include <vexcl/util.hpp>

namespace vex {

namespace generator {

template <bool dummy = true>
class recorder {
    public:
	static void set(std::ostream &s) {
	    os = &s;
	}

	static std::ostream& get() {
	    return os ? *os : std::cout;
	}

    private:
	static std::ostream *os;
};

template <bool dummy>
std::ostream* recorder<dummy>::os = 0;

inline void set_recorder(std::ostream &os) {
    recorder<>::set(os);
}

inline std::ostream& get_recorder() {
    return recorder<>::get();
}

template <class T, class Enable = void>
struct terminal {
    static std::string get(const T &v) {
	std::ostringstream s;
	s << std::scientific << std::setprecision(18) << v;
	return s.str();
    }
};

template <class T>
struct terminal< T, typename std::enable_if<T::is_symbolic>::type > {
    static std::string get(const T &v) {
	return v.get_string();
    }
};

enum symbolic_kind {
    VECTOR,
    SCALAR
};

template <typename T, symbolic_kind kind = VECTOR, bool is_const = false>
class symbolic {
    public:
	static const bool is_symbolic = true;

	symbolic(bool need_declaration = true) : num(index++)
	{
	    if (need_declaration) {
		get_recorder()
		    << type_name<T>() << " " << get_string() << ";\n";
	    }
	}

	symbolic(const symbolic &s) : num(index++) {
	    get_recorder()
		<< type_name<T>() << " " << get_string() << " = "
		<< s.get_string() << ";\n";
	}

	std::string get_string() const {
	    std::ostringstream s;
	    s << "var" << num;
	    return s.str();
	}

	const symbolic& operator=(const symbolic &s) {
	    get_recorder()
		<< get_string() << " = " << s.get_string() << ";\n";
	    return *this;
	}

	template <class Expr>
	const symbolic& operator=(const Expr &expr) const {
	    get_recorder()
		<< get_string() << " = " << terminal<Expr>::get(expr) << ";\n";
	    return *this;
	}

#define COMPOUND_ASSIGNMENT(cop, op) \
	template <class Expr> \
	const symbolic& operator cop(const Expr &expr) { \
	    return *this = *this op expr; \
	}

	COMPOUND_ASSIGNMENT(+=, +);
	COMPOUND_ASSIGNMENT(-=, -);
	COMPOUND_ASSIGNMENT(*=, *);
	COMPOUND_ASSIGNMENT(/=, /);
	COMPOUND_ASSIGNMENT(%=, %);
	COMPOUND_ASSIGNMENT(&=, &);
	COMPOUND_ASSIGNMENT(|=, |);
	COMPOUND_ASSIGNMENT(^=, ^);
	COMPOUND_ASSIGNMENT(<<=, <<);
	COMPOUND_ASSIGNMENT(>>=, >>);

#undef COMPOUND_ASSIGNMENT

	std::string read() const {
	    std::ostringstream s;
	    s << type_name<T>() << " " << get_string() << " = p_" << get_string();

	    switch (kind) {
		case VECTOR:
		    s << "[idx];\n";
		    break;
		case SCALAR:
		    s << ";\n";
		    break;
	    }

	    return s.str();
	}

	std::string write() const {
	    std::ostringstream s;

	    if (kind == VECTOR && !is_const)
		s << "p_" << get_string() << "[idx] = " << get_string() << ";\n";

	    return s.str();
	}

	std::string prmdecl() const {
	    std::ostringstream s;

	    if (kind == VECTOR)
		s << "global ";

	    if (is_const)
		s << "const ";

	    s << type_name<T>();

	    if (kind == VECTOR)
		s << "* ";

	    s << "p_" << get_string();

	    return s.str();
	}
    private:
	static size_t index;
	size_t num;
};

template <typename T, symbolic_kind kind, bool is_const>
size_t symbolic<T, kind, is_const>::index = 0;

template <class LHS, binop::kind OP, class RHS>
struct symbolic_expression {
    static const bool is_symbolic = true;

    symbolic_expression(const LHS &lhs, const RHS &rhs) : lhs(lhs), rhs(rhs) {}

    const LHS &lhs;
    const RHS &rhs;

    std::string get_string() const {
	std::ostringstream s;
	s << "(" << terminal<LHS>::get(lhs) << " " << binop::traits<OP>::oper()
	  << " " << terminal<RHS>::get(rhs) << ")";
	return s.str();
    }
};

template <class T, class Enable = void>
struct valid_symb
    : public std::false_type {};

template <class T>
struct valid_symb<T, typename std::enable_if<std::is_arithmetic<T>::value>::type>
    : std::true_type {};

template <class T>
struct valid_symb<T, typename std::enable_if<T::is_symbolic>::type>
    : std::true_type {};

#define DEFINE_BINARY_OP(kind, oper) \
template <class LHS, class RHS> \
typename std::enable_if<valid_symb<LHS>::value && valid_symb<RHS>::value, \
symbolic_expression<LHS, kind, RHS> \
>::type \
operator oper(const LHS &lhs, const RHS &rhs) { \
    return symbolic_expression<LHS, kind, RHS>(lhs, rhs); \
}

DEFINE_BINARY_OP(binop::Add,          + )
DEFINE_BINARY_OP(binop::Subtract,     - )
DEFINE_BINARY_OP(binop::Multiply,     * )
DEFINE_BINARY_OP(binop::Divide,       / )
DEFINE_BINARY_OP(binop::Remainder,    % )
DEFINE_BINARY_OP(binop::Greater,      > )
DEFINE_BINARY_OP(binop::Less,         < )
DEFINE_BINARY_OP(binop::GreaterEqual, >=)
DEFINE_BINARY_OP(binop::LessEqual,    <=)
DEFINE_BINARY_OP(binop::Equal,        ==)
DEFINE_BINARY_OP(binop::NotEqual,     !=)
DEFINE_BINARY_OP(binop::BitwiseAnd,   & )
DEFINE_BINARY_OP(binop::BitwiseOr,    | )
DEFINE_BINARY_OP(binop::BitwiseXor,   ^ )
DEFINE_BINARY_OP(binop::LogicalAnd,   &&)
DEFINE_BINARY_OP(binop::LogicalOr,    ||)
DEFINE_BINARY_OP(binop::RightShift,   >>)
DEFINE_BINARY_OP(binop::LeftShift,    <<)

#undef DEFINE_BINARY_OP

template <class... Args>
class Kernel {
    public:
	Kernel(
		const std::vector<cl::CommandQueue> &queue,
		const std::string &name, const std::string &body,
		const Args&... args
	      ) : queue(queue)
	{
	    std::ostringstream source;

	    source
		<< standard_kernel_header
		<< "kernel void " << name << "(\n"
		<< "\t" << type_name<size_t>() << " n";

	    declare_params(source, args...);

	    source
		<< "\n\t)\n{\n"
		<< "size_t idx = get_global_id(0);\n"
		<< "if (idx < n) {\n";

	    read_params(source, args...);

	    source << body;
	    
	    write_params(source, args...);

	    source << "}\n}\n";

#ifdef VEXCL_SHOW_KERNELS
	    std::cout << source.str() << std::endl;
#endif

	    for(auto q = queue.begin(); q != queue.end(); q++) {
		cl::Context context = q->getInfo<CL_QUEUE_CONTEXT>();
		cl::Device  device  = q->getInfo<CL_QUEUE_DEVICE>();

		auto program = build_sources(context, source.str());

		krn[context()] = cl::Kernel(program, name.c_str());
		wgs[context()] = kernel_workgroup_size(krn[context()], device);
	    }
	}

	template <class... Param>
	void operator()(const Param&... param) {
	    static_assert(
		    sizeof...(Param) == sizeof...(Args),
		    "Wrong number of kernel parameters"
		    );

	    for(uint d = 0; d < queue.size(); d++) {
		if (size_t psize = prm_size(d, param...)) {
		    cl::Context context = queue[d].getInfo<CL_QUEUE_CONTEXT>();

		    uint pos = 0;
		    krn[context()].setArg(pos++, psize);

		    set_params(krn[context()], d, pos, param...);

		    queue[d].enqueueNDRangeKernel(
			    krn[context()],
			    cl::NullRange,
			    alignup(psize, wgs[context()]),
			    wgs[context()]
			    );
		}
	    }
	}

    private:
	std::vector<cl::CommandQueue> queue;

	std::map<cl_context, cl::Kernel> krn;
	std::map<cl_context, uint> wgs;

	void declare_params(std::ostream &os) const {}

	template <class Head, class... Tail>
	void declare_params(std::ostream &os, const Head &head, const Tail&... tail) {
	    os << ",\n\t" << head.prmdecl();
	    declare_params(os, tail...);
	}

	void read_params(std::ostream &os) const {}

	template <class Head, class... Tail>
	void read_params(std::ostream &os, const Head &head, const Tail&... tail) {
	    os << head.read();
	    read_params(os, tail...);
	}

	void write_params(std::ostream &os) const {}

	template <class Head, class... Tail>
	void write_params(std::ostream &os, const Head &head, const Tail&... tail) {
	    os << head.write();
	    write_params(os, tail...);
	}

	size_t prm_size(uint d) const {
	    throw std::logic_error(
		    "Kernel has to have at least one vector parameter"
		    );
	}

	template <class Head, class... Tail>
	size_t prm_size(uint d, const Head &head, const Tail&... tail) const {
	    if (std::is_arithmetic<Head>::value)
		return prm_size(d, tail...);
	    else 
		return KernelGenerator<Head>(head).part_size(d);
	}

	void set_params(cl::Kernel &k, uint d, uint &p) const {}

	template <class Head, class... Tail>
	void set_params(cl::Kernel &k, uint d, uint &p, const Head &head,
		const Tail&... tail) const
	{
	    KernelGenerator<Head>(head).kernel_args(k, d, p);
	    set_params(k, d, p, tail...);
	}
};

template <class... Args>
Kernel<Args...> build_kernel(
	const std::vector<cl::CommandQueue> &queue,
	const std::string &name, const std::string& body, const Args&... args
	)
{
    return Kernel<Args...>(queue, name, body, args...);
}

} // namespace generator;

} // namespace vex;

#endif
