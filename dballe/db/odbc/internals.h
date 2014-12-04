/*
 * db/odbc/internals - Implementation infrastructure for the ODBC DB connection
 *
 * Copyright (C) 2005--2014  ARPA-SIM <urpsim@smr.arpa.emr.it>
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

#ifndef DBALLE_DB_ODBC_INTERNALS_H
#define DBALLE_DB_ODBC_INTERNALS_H

/** @file
 * @ingroup db
 *
 * Database functions and data structures used by the db module, but not
 * exported as official API.
 */

#include <dballe/db/db.h>
#include <dballe/db/sql.h>
#include <dballe/db/querybuf.h>
#include <sqltypes.h>

// Bit values for server/driver quirks
#define DBA_DB_QUIRK_NO_ROWCOUNT_IN_DIAG (1 << 0)

namespace dballe {
namespace db {
struct ODBCStatement;

// Define this to get warnings when a Statement is closed but its data have not
// all been read yet
// #define DEBUG_WARN_OPEN_TRANSACTIONS

/**
 * Report an ODBC error, using informations from the ODBC diagnostic record
 */
struct error_odbc : public db::error
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
struct ODBCConnection : public Connection
{
    /** ODBC database connection */
    SQLHDBC od_conn;
    /** True if the connection is open */
    bool connected;
    /// Bitfield of quirks we should be aware of when using ODBC
    unsigned server_quirks;

protected:
    /** Precompiled LAST_INSERT_ID (or equivalent) SQL statement */
    ODBCStatement* stm_last_insert_id;
    /** ID of the last autogenerated primary key */
    int m_last_insert_id;

public:
    ODBCConnection();
    ODBCConnection(const ODBCConnection&) = delete;
    ODBCConnection(const ODBCConnection&&) = delete;
    ~ODBCConnection();

    ODBCConnection& operator=(const ODBCConnection&) = delete;

    void connect(const char* dsn, const char* user, const char* password);
    void connect_file(const std::string& fname);
    void driver_connect(const char* config);
    std::string driver_name();
    std::string driver_version();
    void get_info(SQLUSMALLINT info_type, SQLINTEGER& res);
    void set_autocommit(bool val);

    std::unique_ptr<Transaction> transaction() override;
    std::unique_ptr<Statement> statement() override;
    std::unique_ptr<ODBCStatement> odbcstatement();

    void exec(const std::string& query) override;

    /// Check if the database contains a table
    bool has_table(const std::string& name) override;

    /**
     * Get a value from the settings table.
     *
     * Returns the empty string if the table does not exist.
     */
    std::string get_setting(const std::string& key) override;

    /**
     * Set a value in the settings table.
     *
     * The table is created if it does not exist.
     */
    void set_setting(const std::string& key, const std::string& value) override;

    /// Drop the settings table
    void drop_settings() override;

    /**
     * Delete a table in the database if it exists, otherwise do nothing.
     */
    void drop_table_if_exists(const char* name) override;

    /**
     * Delete a sequence in the database if it exists, otherwise do nothing.
     */
    void drop_sequence_if_exists(const char* name) override;

    /**
     * Return LAST_INSERT_ID or LAST_INSER_ROWID or whatever is appropriate for
     * the current database, if supported.
     *
     * If not supported, an exception is thrown.
     */
    int get_last_insert_id() override;

    void add_datetime(Querybuf& qb, const int* dt) const override;

protected:
    void init_after_connect();
};

/// ODBC statement
struct ODBCStatement : public Statement
{
    const ODBCConnection& conn;
    SQLHSTMT stm = nullptr;
    /// If non-NULL, ignore all errors with this code
    const char* ignore_error = nullptr;
#ifdef DEBUG_WARN_OPEN_TRANSACTIONS
    /// Debugging aids: dump query to stderr if destructor is called before fetch returned SQL_NO_DATA
    std::string debug_query;
    bool debug_reached_completion = false;
#endif

    ODBCStatement(ODBCConnection& conn);
    ODBCStatement(const ODBCStatement&) = delete;
    ODBCStatement(const ODBCStatement&&) = delete;
    ~ODBCStatement();
    ODBCStatement& operator=(const ODBCStatement&) = delete;

    void bind_in(int idx, const int& val) override;
    void bind_in(int idx, const int& val, const SQLLEN& ind);
    void bind_in(int idx, const unsigned& val) override;
    void bind_in(int idx, const unsigned& val, const SQLLEN& ind);
    void bind_in(int idx, const unsigned short& val) override;
    void bind_in(int idx, const unsigned short& val, const SQLLEN& ind);
    void bind_in(int idx, const char* val) override;
    void bind_in(int idx, const char* val, const SQLLEN& ind);
    void bind_in(int idx, const SQL_TIMESTAMP_STRUCT& val);

    void bind_out(int idx, int& val);
    void bind_out(int idx, int& val, SQLLEN& ind);
    void bind_out(int idx, unsigned& val);
    void bind_out(int idx, unsigned& val, SQLLEN& ind);
    void bind_out(int idx, unsigned short& val);
    void bind_out(int idx, unsigned short& val, SQLLEN& ind);
    void bind_out(int idx, char* val, SQLLEN buflen);
    void bind_out(int idx, char* val, SQLLEN buflen, SQLLEN& ind);
    void bind_out(int idx, SQL_TIMESTAMP_STRUCT& val);
    void bind_out(int idx, SQL_TIMESTAMP_STRUCT& val, SQLLEN& ind);

    void prepare(const char* query);
    void prepare(const char* query, int qlen);
    void prepare(const std::string& query) override;

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

    void execute_ignoring_results() override;

    /**
     * @return the number of columns in the result set (or 0 if the statement
     * did not return columns)
     */
    int columns_count();
    bool fetch();
    bool fetch_expecting_one();
    void close_cursor();
    void close_cursor_if_needed();
    /// Row count for select operations
    size_t select_rowcount();
    /// Row count for insert, delete and other non-select operations
    size_t rowcount();

    void set_cursor_forward_only();
    void set_cursor_static();

protected:
    bool error_is_ignored();
    bool is_error(int sqlres);
};

/// ODBC statement to read a sequence
struct Sequence : public ODBCStatement
{
    int out;

    Sequence(ODBCConnection& conn, const char* name);
    ~Sequence();

    /// Read the current value of the sequence
    const int& read();

private:
    // disallow copy
    Sequence(const Sequence&);
    Sequence& operator=(const Sequence&);
};

static inline bool operator!=(const SQL_TIMESTAMP_STRUCT& a, const SQL_TIMESTAMP_STRUCT& b)
{
    return a.year != b.year || a.month != b.month || a.day != b.day || a.hour != b.hour || a.minute != b.minute || a.second != b.second || a.fraction != b.fraction;
}

std::ostream& operator<<(std::ostream& o, const SQL_TIMESTAMP_STRUCT& t);

static inline SQL_TIMESTAMP_STRUCT make_sql_timestamp(int year, int month, int day, int hour, int minute, int second)
{
    SQL_TIMESTAMP_STRUCT res;
    res.year = year;
    res.month = month;
    res.day = day;
    res.hour = hour;
    res.minute = minute;
    res.second = second;
    res.fraction = 0;
    return res;
}


} // namespace db
} // namespace dballe

/* vim:set ts=4 sw=4: */
#endif
