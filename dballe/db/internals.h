/*
 * db/internals - Internal support infrastructure for the DB
 *
 * Copyright (C) 2005--2010  ARPA-SIM <urpsim@smr.arpa.emr.it>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 *
 * Author: Enrico Zini <enrico@enricozini.com>
 */

#ifndef DBALLE_DB_INTERNALS_H
#define DBALLE_DB_INTERNALS_H

/** @file
 * @ingroup db
 *
 * Database functions and data structures used by the db module, but not
 * exported as official API.
 */

#include <dballe/db/querybuf.h>
#include <dballe/db/odbcworkarounds.h>
#include <wreport/error.h>

#include <sqltypes.h>

/*
 * Define to true to enable the use of transactions during writes
 */
#define DBA_USE_TRANSACTIONS

/* Define this to enable referential integrity */
#undef USE_REF_INT

namespace dballe {
namespace db {

/** Trace macros internally used for debugging
 * @{
 */

// #define TRACE_DB

#ifdef TRACE_DB
#define TRACE(...) fprintf(stderr, __VA_ARGS__)
#define IFTRACE if (1)
#else
/** Ouput a trace message */
#define TRACE(...) do { } while (0)
/** Prefix a block of code to compile only if trace is enabled */
#define IFTRACE if (0)
#endif

/** @} */

// Define this to get warnings when a Statement is closed but its data have not
// all been read yet
// #define DEBUG_WARN_OPEN_TRANSACTIONS

/**
 * Report an ODBC error, using informations from the ODBC diagnostic record
 */
struct error_odbc : public wreport::error 
{
    std::string msg;

    /**
     * Copy informations from the ODBC diagnostic record to the dba error
     * report
     */
    error_odbc(SQLSMALLINT handleType, SQLHANDLE handle, const std::string& msg);
    ~error_odbc() throw () {}

    wreport::ErrorCode code() const throw () { return wreport::WR_ERR_ODBC; }

    virtual const char* what() const throw () { return msg.c_str(); }

    static void throwf(SQLSMALLINT handleType, SQLHANDLE handle, const char* fmt, ...) WREPORT_THROWF_ATTRS(3, 4);
};

/**
 * Supported SQL servers.
 */
enum ServerType
{
    MYSQL,
    SQLITE,
    ORACLE,
    POSTGRES,
};

/// ODBC environment
struct Environment
{
    SQLHENV od_env;

    Environment();
    ~Environment();

    static Environment& get();

private:
    // disallow copy
    Environment(const Environment&);
    Environment& operator=(const Environment&);
};

/// Database connection
struct Connection
{
    /** ODBC database connection */
    SQLHDBC od_conn;
    /** True if the connection is open */
    bool connected;
    /** Type of SQL server we are connected to */
    enum ServerType server_type;

    Connection();
    ~Connection();

    void connect(const char* dsn, const char* user, const char* password);
    void driver_connect(const char* config);
    std::string driver_name();
    void set_autocommit(bool val);

    /// Commit a transaction
    void commit();

    /// Rollback a transaction
    void rollback();

	/**
	 * Delete a table in the database if it exists, otherwise do nothing.
	 */
	void drop_table_if_exists(const char* name);

	/**
	 * Delete a sequence in the database if it exists, otherwise do nothing.
	 */
	void drop_sequence_if_exists(const char* name);

protected:
    void init_after_connect();

private:
    // disallow copy
    Connection(const Connection&);
    Connection& operator=(const Connection&);
};

/// RAII transaction
struct Transaction
{
    Connection& conn;
    bool fired;

    Transaction(Connection& conn) : conn(conn), fired(false) {}
    ~Transaction() { if (!fired) rollback(); }

    void commit() { conn.commit(); fired = true; }
    void rollback() { conn.rollback(); fired = true; }
};

/// ODBC statement
struct Statement
{
    //Connection& conn;
    SQLHSTMT stm;
    /// If non-NULL, ignore all errors with this code
    const char* ignore_error;
#ifdef DEBUG_WARN_OPEN_TRANSACTIONS
    /// Debugging aids: dump query to stderr if destructor is called before fetch returned SQL_NO_DATA
    std::string debug_query;
    bool debug_reached_completion;
#endif

    Statement(Connection& conn);
    ~Statement();

    void bind_in(int idx, const DBALLE_SQL_C_SINT_TYPE& val);
    void bind_in(int idx, const DBALLE_SQL_C_SINT_TYPE& val, const SQLLEN& ind);
    void bind_in(int idx, const DBALLE_SQL_C_UINT_TYPE& val);
    void bind_in(int idx, const DBALLE_SQL_C_UINT_TYPE& val, const SQLLEN& ind);
    void bind_in(int idx, const unsigned short& val);
    void bind_in(int idx, const char* val);
    void bind_in(int idx, const char* val, const SQLLEN& ind);
    void bind_in(int idx, const SQL_TIMESTAMP_STRUCT& val);

    void bind_out(int idx, DBALLE_SQL_C_SINT_TYPE& val);
    void bind_out(int idx, DBALLE_SQL_C_SINT_TYPE& val, SQLLEN& ind);
    void bind_out(int idx, DBALLE_SQL_C_UINT_TYPE& val);
    void bind_out(int idx, DBALLE_SQL_C_UINT_TYPE& val, SQLLEN& ind);
    void bind_out(int idx, unsigned short& val);
    void bind_out(int idx, char* val, SQLLEN buflen);
    void bind_out(int idx, char* val, SQLLEN buflen, SQLLEN& ind);
    void bind_out(int idx, SQL_TIMESTAMP_STRUCT& val);

    void prepare(const char* query);
    void prepare(const char* query, int qlen);

    /// @return SQLExecute's result
    int execute();
    /// @return SQLExecute's result
    int exec_direct(const char* query);
    /// @return SQLExecute's result
    int exec_direct(const char* query, int qlen);

    /// @return SQLExecute's result
    int execute_and_close();
    /// @return SQLExecute's result
    int exec_direct_and_close(const char* query);
    /// @return SQLExecute's result
    int exec_direct_and_close(const char* query, int qlen);

    /**
     * @return the number of columns in the result set (or 0 if the statement
     * did not return columns)
     */
    int columns_count();
    bool fetch();
    bool fetch_expecting_one();
    void close_cursor();
    size_t rowcount();

    void set_cursor_forward_only();

protected:
    bool error_is_ignored();
    bool is_error(int sqlres);

private:
    // disallow copy
    Statement(const Statement&);
    Statement& operator=(const Statement&);
};

/// ODBC statement to read a sequence
struct Sequence : public Statement
{
    DBALLE_SQL_C_SINT_TYPE out;

    Sequence(Connection& conn, const char* name);
    ~Sequence();

    /// Read the current value of the sequence
    const DBALLE_SQL_C_SINT_TYPE& read();

private:
    // disallow copy
    Sequence(const Sequence&);
    Sequence& operator=(const Sequence&);
};

/// Return the default repinfo file pathname
const char* default_repinfo_file();

} // namespace db
} // namespace dballe

/* vim:set ts=4 sw=4: */
#endif
