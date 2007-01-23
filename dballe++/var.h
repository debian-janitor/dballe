#ifndef DBALLE_CPP_VAR_H
#define DBALLE_CPP_VAR_H

#include <dballe/core/var.h>

#include <dballe++/error.h>

namespace dballe {

/**
 * Wrap a dba_varinfo
 */
class Varinfo
{
	dba_varinfo m_info;
	
public:
	Varinfo(dba_varinfo info) : m_info(info) {}

	/** The variable code. */
	dba_varcode var() const { return m_info->var; }
	/** The variable description. */
	const char* desc() const { return m_info->desc; }
	/** The measurement unit of the variable. */
	const char* unit() const { return m_info->unit; }
	/** The scale of the variable.  When the variable is represented as an
	 * integer, it is multiplied by 10**scale */
	int scale() const { return m_info->scale; }
	/** The reference value for the variable.  When the variable is represented
	 * as an integer, and after scaling, it is added this value */
	int ref() const { return m_info->ref; }
	/** The length in digits of the integer representation of this variable
	 * (after scaling and changing reference value) */
	int len() const { return m_info->len; }
	/** The reference value for bit-encoding.  When the variable is encoded in
	 * a bit string, it is added this value */
	int bit_ref() const { return m_info->bit_ref; }
	/** The length in bits of the variable when encoded in a bit string (after
	 * scaling and changing reference value) */
	int bit_len() const { return m_info->bit_len; }
	/** True if the variable is a string; false if it is a numeric value */
	bool is_string() const { return m_info->is_string != 0; }
	/** Minimum unscaled value the field can have */
	int imin() const { return m_info->imin; }
	/** Maximum unscaled value the field can have */
	int imax() const { return m_info->imax; }
	/** Minimum scaled value the field can have */
	double dmin() const { return m_info->dmin; }
	/** Maximum scaled value the field can have */
	double dmax() const { return m_info->dmax; }

	/** Create a Varinfo from a dba_varcode using the local B table */
	static Varinfo create(dba_varcode code);
};

/**
 * Wrap a dba_var
 */
class Var
{
	dba_var m_var;

public:
	/// Wraps an existing dba_var, taking charge of memory allocation
	Var(dba_var var) : m_var(var) {}

	/// Wraps an existing dba_var, copying it
	static Var clone(dba_var var);

	/// Copy constructor
	Var(const Var& var)
	{
		checked(dba_var_copy(var.var(), &m_var));
	}
	/// Create a Var from a dba_varcode using the local B table
	Var(dba_varcode code)
	{
		checked(dba_var_create_local(code, &m_var));
	}
	/// Create a Var from a dba_info
	Var(dba_varinfo info)
	{
		checked(dba_var_create(info, &m_var));
	}
	/// Create a Var from a dba_info and an integer value
	Var(dba_varinfo info, int val)
	{
		checked(dba_var_createi(info, val, &m_var));
	}
	/// Create a Var from a dba_info and a double value
	Var(dba_varinfo info, double val)
	{
		checked(dba_var_created(info, val, &m_var));
	}
	/// Create a Var from a dba_info and a string
	Var(dba_varinfo info, const char* val)
	{
		checked(dba_var_createc(info, val, &m_var));
	}
	/// Create a Var from a dba_info and a string
	Var(dba_varinfo info, const std::string& val)
	{
		checked(dba_var_createc(info, val.c_str(), &m_var));
	}
	~Var()
	{
		if (m_var)
			dba_var_delete(m_var);
	}

	/// Assignment with copy semantics
	Var& operator=(const Var& var)
	{
		dba_var_delete(m_var);
		m_var = 0;
		checked(dba_var_copy(var.var(), &m_var));
		return *this;
	}

	/// Create a copy of this variable
	Var copy() const
	{
		dba_var res;
		checked(dba_var_copy(m_var, &res));
		return Var(res);
	}

	/// Check if two variables have the same value
	bool equals(const Var& var) const
	{
		return dba_var_equals(m_var, var.var()) == 1;
	}

	/// Get the variable value, as an unscaled integer
	int enqi()
	{
		int res;
		checked(dba_var_enqi(m_var, &res));
		return res;
	}
	/// Get the variable value, as a double
	double enqd()
	{
		double res;
		checked(dba_var_enqd(m_var, &res));
		return res;
	}
	/// Get the variable value, as a string
	const char* enqc()
	{
		const char* res;
		checked(dba_var_enqc(m_var, &res));
		return res;
	}
	/// Get the variable value, as a string
	std::string enqs()
	{
		const char* res;
		checked(dba_var_enqc(m_var, &res));
		return res;
	}

	/// Get the variable value, from an unscaled integer
	void set(int val)
	{
		checked(dba_var_seti(m_var, val));
	}
	/// Get the variable value, from a double
	void set(double val)
	{
		checked(dba_var_setd(m_var, val));
	}
	/// Get the variable value, from a string
	void set(const char* val)
	{
		checked(dba_var_setc(m_var, val));
	}
	/// Get the variable value, from a string
	void set(const std::string& val)
	{
		checked(dba_var_setc(m_var, val.c_str()));
	}

	/// Unset the variable value
	void unset()
	{
		checked(dba_var_unset(m_var));
	}

	/// Get the variable code
	dba_varcode code()
	{
		return dba_var_code(m_var);
	}
	
	/// Get the variable Varinfo metadata
	Varinfo info()
	{
		return Varinfo(dba_var_info(m_var));
	}

	/// Create a formatted string representation of the variable value
	std::string format(const std::string& nullValue = "(undef)")
	{
		if (dba_var_value(m_var) == NULL)
			return nullValue;
		else if (dba_var_info(m_var)->is_string)
			return dba_var_value(m_var);
		else
		{
			dba_varinfo info = dba_var_info(m_var);
			char buf[20];
			snprintf(buf, 20, "%.*f", info->scale > 0 ? info->scale : 0, enqd());
			return buf;
		}
	}
	
	/// Get the raw, string-serialised variable value
	const char* raw()
	{
		return dba_var_value(m_var);
	}

	/// Access the underlying dba_var
	const dba_var var() const
	{
		return m_var;
	}
	/// Access the underlying dba_var
	dba_var var()
	{
		return m_var;
	}
};

}

#endif
