/*
 * db/sqlite/internals - Implementation infrastructure for the SQLite DB connection
 *
 * Copyright (C) 2014  ARPA-SIM <urpsim@smr.arpa.emr.it>
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

#include "internals.h"

#if 0
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdlib>
#include <limits.h>
#include <unistd.h>
#include "dballe/core/vasprintf.h"
#include "dballe/core/verbose.h"
#include "dballe/db/querybuf.h"

#include <iostream>
#endif

using namespace std;
using namespace wreport;

namespace dballe {
namespace db {

error_sqlite::error_sqlite(sqlite3* db, const std::string& msg)
{
    this->msg = msg;
    this->msg += ":";
    this->msg += sqlite3_errmsg(db);
}

void error_sqlite::throwf(sqlite3* db, const char* fmt, ...)
{
    // Format the arguments
    va_list ap;
    va_start(ap, fmt);
    char* cmsg;
    vasprintf(&cmsg, fmt, ap);
    va_end(ap);

    // Convert to string
    std::string msg(cmsg);
    free(cmsg);
    throw error_sqlite(db, msg);
}

#if 0
ODBCConnection::ODBCConnection()
    : connected(false), server_quirks(0), stm_last_insert_id(0)
{
    /* Allocate the ODBC connection handle */
    Environment& env = Environment::get();
    int sqlres = SQLAllocHandle(SQL_HANDLE_DBC, env.od_env, &od_conn);
    if ((sqlres != SQL_SUCCESS) && (sqlres != SQL_SUCCESS_WITH_INFO))
        throw error_odbc(SQL_HANDLE_DBC, od_conn, "Allocating new connection handle");
}

ODBCConnection::~ODBCConnection()
{
    if (stm_last_insert_id) delete stm_last_insert_id;
    if (connected)
    {
        int sqlres = SQLEndTran(SQL_HANDLE_DBC, od_conn, SQL_ROLLBACK);
        if ((sqlres != SQL_SUCCESS) && (sqlres != SQL_SUCCESS_WITH_INFO))
            verbose_odbc_error(SQL_HANDLE_DBC, od_conn, "Cannot close existing transactions");

        sqlres = SQLDisconnect(od_conn);
        if ((sqlres != SQL_SUCCESS) && (sqlres != SQL_SUCCESS_WITH_INFO))
            verbose_odbc_error(SQL_HANDLE_DBC, od_conn, "Cannot disconnect from database");
    }
    int sqlres = SQLFreeHandle(SQL_HANDLE_DBC, od_conn);
    if ((sqlres != SQL_SUCCESS) && (sqlres != SQL_SUCCESS_WITH_INFO))
        verbose_odbc_error(SQL_HANDLE_DBC, od_conn, "Cannot free handle");
}

void ODBCConnection::connect(const char* dsn, const char* user, const char* password)
{
    /* Connect to the DSN */
    int sqlres = SQLConnect(od_conn,
                (SQLCHAR*)dsn, SQL_NTS,
                (SQLCHAR*)user, SQL_NTS,
                (SQLCHAR*)(password == NULL ? "" : password), SQL_NTS);
    if ((sqlres != SQL_SUCCESS) && (sqlres != SQL_SUCCESS_WITH_INFO))
        error_odbc::throwf(SQL_HANDLE_DBC, od_conn, "Connecting to DSN %s as user %s", dsn, user);
    init_after_connect();
}

void ODBCConnection::connect_file(const std::string& fname)
{
    // Access sqlite file directly
    string buf;
    if (fname[0] != '/')
    {
        char cwd[PATH_MAX];
        buf = "Driver=SQLite3;Database=";
        buf += getcwd(cwd, PATH_MAX);
        buf += "/";
        buf += fname;
        buf += ";";
    }
    else
    {
        buf = "Driver=SQLite3;Database=";
        buf += fname;
        buf += ";";
    }
    driver_connect(buf.c_str());
}

void ODBCConnection::driver_connect(const char* config)
{
    /* Connect to the DSN */
    char sdcout[1024];
    SQLSMALLINT outlen;
    int sqlres = SQLDriverConnect(od_conn, NULL,
                    (SQLCHAR*)config, SQL_NTS,
                    (SQLCHAR*)sdcout, 1024, &outlen,
                    SQL_DRIVER_NOPROMPT);

    if ((sqlres != SQL_SUCCESS) && (sqlres != SQL_SUCCESS_WITH_INFO))
        error_odbc::throwf(SQL_HANDLE_DBC, od_conn, "Connecting to DB using configuration %s", config);
    init_after_connect();
}

void ODBCConnection::init_after_connect()
{
    /* Find out what kind of database we are working with */
    string name = driver_name();

    if (name.substr(0, 9) == "libmyodbc" || name.substr(0, 6) == "myodbc")
    {
        server_type = ServerType::MYSQL;
        // MariaDB in at least one version returns 0 for rowcount in SQL_DIAG_CURSOR_ROW_COUNT
        server_quirks = DBA_DB_QUIRK_NO_ROWCOUNT_IN_DIAG;
    }
    else if (name.substr(0, 6) == "sqlite")
    {
        string version = driver_version();
        server_type = ServerType::SQLITE;
        if (version < "0.99")
            server_quirks = DBA_DB_QUIRK_NO_ROWCOUNT_IN_DIAG;
    }
    else if (name.substr(0, 5) == "SQORA")
        server_type = ServerType::ORACLE;
    else if (name.substr(0, 11) == "libpsqlodbc" || name.substr(0, 8) == "psqlodbc")
        server_type = ServerType::POSTGRES;
    else
    {
        fprintf(stderr, "ODBC driver %s is unsupported: assuming it's similar to Postgres", name.c_str());
        server_type = ServerType::POSTGRES;
    }

    connected = true;

    IFTRACE {
        SQLINTEGER v;
        get_info(SQL_DYNAMIC_CURSOR_ATTRIBUTES2, v);
        if (v & SQL_CA2_CRC_EXACT) TRACE("SQL_DYNAMIC_CURSOR_ATTRIBUTES2 SQL_CA2_CRC_EXACT\n");
        if (v & SQL_CA2_CRC_APPROXIMATE) TRACE("SQL_DYNAMIC_CURSOR_ATTRIBUTES2 SQL_CA2_CRC_APPROXIMATE\n");
        get_info(SQL_FORWARD_ONLY_CURSOR_ATTRIBUTES2, v);
        if (v & SQL_CA2_CRC_EXACT) TRACE("SQL_FORWARD_ONLY_CURSOR_ATTRIBUTES2 SQL_CA2_CRC_EXACT\n");
        if (v & SQL_CA2_CRC_APPROXIMATE) TRACE("SQL_FORWARD_ONLY_CURSOR_ATTRIBUTES2 SQL_CA2_CRC_APPROXIMATE\n");

        get_info(SQL_KEYSET_CURSOR_ATTRIBUTES2, v);
        if (v & SQL_CA2_CRC_EXACT) TRACE("SQL_KEYSET_CURSOR_ATTRIBUTES2, SQL_CA2_CRC_EXACT\n");
        if (v & SQL_CA2_CRC_APPROXIMATE) TRACE("SQL_KEYSET_CURSOR_ATTRIBUTES2, SQL_CA2_CRC_APPROXIMATE\n");
        get_info(SQL_STATIC_CURSOR_ATTRIBUTES2, v);
        if (v & SQL_CA2_CRC_EXACT) TRACE("SQL_STATIC_CURSOR_ATTRIBUTES2, SQL_CA2_CRC_EXACT\n");
        if (v & SQL_CA2_CRC_APPROXIMATE) TRACE("SQL_STATIC_CURSOR_ATTRIBUTES2, SQL_CA2_CRC_APPROXIMATE\n");
    }

    set_autocommit(false);
}

std::string ODBCConnection::driver_name()
{
    char drivername[50];
    SQLSMALLINT len;
    int sqlres = SQLGetInfo(od_conn, SQL_DRIVER_NAME, (SQLPOINTER)drivername, 50, &len);
    if ((sqlres != SQL_SUCCESS) && (sqlres != SQL_SUCCESS_WITH_INFO))
        throw error_odbc(SQL_HANDLE_DBC, od_conn, "Getting ODBC driver name");
    return string(drivername, len);
}

std::string ODBCConnection::driver_version()
{
    char driverver[50];
    SQLSMALLINT len;
    int sqlres = SQLGetInfo(od_conn, SQL_DRIVER_VER, (SQLPOINTER)driverver, 50, &len);
    if ((sqlres != SQL_SUCCESS) && (sqlres != SQL_SUCCESS_WITH_INFO))
        throw error_odbc(SQL_HANDLE_DBC, od_conn, "Getting ODBC driver version");
    return string(driverver, len);
}

void ODBCConnection::get_info(SQLUSMALLINT info_type, SQLINTEGER& res)
{
    int sqlres = SQLGetInfo(od_conn, info_type, &res, 0, 0);
    if ((sqlres != SQL_SUCCESS) && (sqlres != SQL_SUCCESS_WITH_INFO))
        throw error_odbc(SQL_HANDLE_DBC, od_conn, "Getting ODBC driver information");
}

void ODBCConnection::set_autocommit(bool val)
{
    int sqlres = SQLSetConnectAttr(od_conn, SQL_ATTR_AUTOCOMMIT, (void*)(val ? SQL_AUTOCOMMIT_ON : SQL_AUTOCOMMIT_OFF), 0);
    if ((sqlres != SQL_SUCCESS) && (sqlres != SQL_SUCCESS_WITH_INFO))
        error_odbc::throwf(SQL_HANDLE_DBC, od_conn, "%s ODBC autocommit", val ? "Enabling" : "Disabling");
}

struct ODBCTransaction : public Transaction
{
    SQLHDBC od_conn;
    bool fired = false;

    ODBCTransaction(SQLHDBC conn) : od_conn(conn) {}
    ~ODBCTransaction() { if (!fired) rollback(); }

    void commit() override
    {
        int sqlres = SQLEndTran(SQL_HANDLE_DBC, od_conn, SQL_COMMIT);
        if ((sqlres != SQL_SUCCESS) && (sqlres != SQL_SUCCESS_WITH_INFO))
            throw error_odbc(SQL_HANDLE_DBC, od_conn, "Committing a transaction");
        fired = true;
    }
    void rollback() override
    {
        int sqlres = SQLEndTran(SQL_HANDLE_DBC, od_conn, SQL_ROLLBACK);
        if ((sqlres != SQL_SUCCESS) && (sqlres != SQL_SUCCESS_WITH_INFO))
            throw error_odbc(SQL_HANDLE_DBC, od_conn, "Rolling back a transaction");
        fired = true;
    }
};

std::unique_ptr<Transaction> ODBCConnection::transaction()
{
    return unique_ptr<Transaction>(new ODBCTransaction(od_conn));
}

std::unique_ptr<Statement> ODBCConnection::statement()
{
    return unique_ptr<Statement>(new ODBCStatement(*this));
}

std::unique_ptr<ODBCStatement> ODBCConnection::odbcstatement()
{
    return unique_ptr<ODBCStatement>(new ODBCStatement(*this));
}

void ODBCConnection::impl_exec_noargs(const std::string& query)
{
    ODBCStatement stm(*this);
    stm.exec_direct_and_close(query.c_str());
}

#define DBA_ODBC_MISSING_TABLE_POSTGRES "42P01"
#define DBA_ODBC_MISSING_TABLE_MYSQL "42S01"
#define DBA_ODBC_MISSING_TABLE_SQLITE "HY000"
#define DBA_ODBC_MISSING_TABLE_ORACLE "42S02"

void ODBCConnection::drop_table_if_exists(const char* name)
{
    switch (server_type)
    {
        case ServerType::MYSQL:
        case ServerType::POSTGRES:
        case ServerType::SQLITE:
            exec(string("DROP TABLE IF EXISTS ") + name);
            break;
        case ServerType::ORACLE:
        {
            auto stm = odbcstatement();
            char buf[100];
            int len;
            stm->ignore_error = DBA_ODBC_MISSING_TABLE_ORACLE;
            len = snprintf(buf, 100, "DROP TABLE %s", name);
            stm->exec_direct_and_close(buf, len);
            break;
        }
    }
}

#define DBA_ODBC_MISSING_SEQUENCE_ORACLE "HY000"
#define DBA_ODBC_MISSING_SEQUENCE_POSTGRES "42P01"
void ODBCConnection::drop_sequence_if_exists(const char* name)
{
    switch (server_type)
    {
        case ServerType::POSTGRES:
            exec(string("DROP SEQUENCE IF EXISTS ") + name);
            break;
        case ServerType::ORACLE:
        {
            auto stm = odbcstatement();
            char buf[100];
            int len;
            stm->ignore_error = DBA_ODBC_MISSING_SEQUENCE_ORACLE;
            len = snprintf(buf, 100, "DROP SEQUENCE %s", name);
            stm->exec_direct_and_close(buf, len);
            break;
        }
        default:
            break;
    }
}

int ODBCConnection::get_last_insert_id()
{
    // Compile the query on demand
    if (!stm_last_insert_id)
    {
        switch (server_type)
        {
            case ServerType::MYSQL:
                stm_last_insert_id = odbcstatement().release();
                stm_last_insert_id->bind_out(1, m_last_insert_id);
                stm_last_insert_id->prepare("SELECT LAST_INSERT_ID()");
                break;
            case ServerType::SQLITE:
                stm_last_insert_id = odbcstatement().release();
                stm_last_insert_id->bind_out(1, m_last_insert_id);
                stm_last_insert_id->prepare("SELECT LAST_INSERT_ROWID()");
                break;
            case ServerType::POSTGRES:
                stm_last_insert_id = odbcstatement().release();
                stm_last_insert_id->bind_out(1, m_last_insert_id);
                stm_last_insert_id->prepare("SELECT LASTVAL()");
                break;
            default:
                throw error_consistency("get_last_insert_id called on a database that does not support it");
        }
    }

    stm_last_insert_id->execute();
    if (!stm_last_insert_id->fetch_expecting_one())
        throw error_consistency("no last insert ID value returned from database");
    return m_last_insert_id;
}

bool ODBCConnection::has_table(const std::string& name)
{
    auto stm = odbcstatement();
    int count;

    switch (server_type)
    {
        case ServerType::MYSQL:
            stm->prepare("SELECT COUNT(*) FROM information_schema.tables WHERE table_schema=DATABASE() AND table_name=?");
            stm->bind_in(1, name.data(), name.size());
            stm->bind_out(1, count);
            break;
        case ServerType::SQLITE:
            stm->prepare("SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name=?");
            stm->bind_in(1, name.data(), name.size());
            stm->bind_out(1, count);
            break;
        case ServerType::ORACLE:
            stm->prepare("SELECT COUNT(*) FROM user_tables WHERE table_name=UPPER(?)");
            stm->bind_in(1, name.data(), name.size());
            stm->bind_out(1, count);
            break;
        case ServerType::POSTGRES:
            stm->prepare("SELECT COUNT(*) FROM information_schema.tables WHERE table_name=?");
            stm->bind_in(1, name.data(), name.size());
            stm->bind_out(1, count);
            break;
    }
    stm->execute();
    stm->fetch_expecting_one();
    return count > 0;
}

std::string ODBCConnection::get_setting(const std::string& key)
{
    if (!has_table("dballe_settings"))
        return string();

    char result[64];
    SQLLEN result_len;

    auto stm = odbcstatement();
    if (server_type == ServerType::MYSQL)
        stm->prepare("SELECT value FROM dballe_settings WHERE `key`=?");
    else
        stm->prepare("SELECT value FROM dballe_settings WHERE \"key\"=?");
    stm->bind_in(1, key.data(), key.size());
    stm->bind_out(1, result, 64, result_len);
    stm->execute();
    string res;
    while (stm->fetch())
        res = string(result, result_len);
    // rtrim string
    size_t n = res.substr(0, 63).find_last_not_of(' ');
    if (n != string::npos)
        res.erase(n+1);
    return res;
}

void ODBCConnection::set_setting(const std::string& key, const std::string& value)
{
    auto stm = odbcstatement();

    if (!has_table("dballe_settings"))
    {
        if (server_type == ServerType::MYSQL)
            exec("CREATE TABLE dballe_settings (`key` CHAR(64) NOT NULL PRIMARY KEY, value CHAR(64) NOT NULL)");
        else
            exec("CREATE TABLE dballe_settings (\"key\" CHAR(64) NOT NULL PRIMARY KEY, value CHAR(64) NOT NULL)");
    }

    // Remove if it exists
    if (server_type == ServerType::MYSQL)
        stm->prepare("DELETE FROM dballe_settings WHERE `key`=?");
    else
        stm->prepare("DELETE FROM dballe_settings WHERE \"key\"=?");
    stm->bind_in(1, key.data(), key.size());
    stm->execute_and_close();

    // Then insert it
    if (server_type == ServerType::MYSQL)
        stm->prepare("INSERT INTO dballe_settings (`key`, value) VALUES (?, ?)");
    else
        stm->prepare("INSERT INTO dballe_settings (\"key\", value) VALUES (?, ?)");
    SQLLEN key_size = key.size();
    SQLLEN value_size = value.size();
    stm->bind_in(1, key.data(), key_size);
    stm->bind_in(2, value.data(), value_size);
    stm->execute_and_close();
}

void ODBCConnection::drop_settings()
{
    drop_table_if_exists("dballe_settings");
}

void ODBCConnection::add_datetime(Querybuf& qb, const int* dt) const
{
    qb.appendf("{ts '%04d-%02d-%02d %02d:%02d:%02d'}", dt[0], dt[1], dt[2], dt[3], dt[4], dt[5]);
}


ODBCStatement::ODBCStatement(ODBCConnection& conn)
    : conn(conn)
{
    int sqlres = SQLAllocHandle(SQL_HANDLE_STMT, conn.od_conn, &stm);
    if ((sqlres != SQL_SUCCESS) && (sqlres != SQL_SUCCESS_WITH_INFO))
        throw error_odbc(SQL_HANDLE_STMT, stm, "Allocating new statement handle");
}

ODBCStatement::~ODBCStatement()
{
#ifdef DEBUG_WARN_OPEN_TRANSACTIONS
    if (!debug_reached_completion)
    {
        string msg("Statement " + debug_query + " destroyed before reaching completion");
        fprintf(stderr, "-- %s\n", msg.c_str());
        //throw error_consistency(msg)
        SQLCloseCursor(stm);
    }
#endif
    SQLFreeHandle(SQL_HANDLE_STMT, stm);
}

bool ODBCStatement::error_is_ignored()
{
    if (!ignore_error) return false;

    // Retrieve the current error code
    char stat[10];
    SQLINTEGER err;
    SQLSMALLINT mlen;
    SQLGetDiagRec(SQL_HANDLE_STMT, stm, 1, (unsigned char*)stat, &err, NULL, 0, &mlen);

    // Ignore the given SQL error
    return memcmp(stat, ignore_error, 5) == 0;
}

bool ODBCStatement::is_error(int sqlres)
{
    return (sqlres != SQL_SUCCESS)
        && (sqlres != SQL_SUCCESS_WITH_INFO)
        && (sqlres != SQL_NO_DATA)
        && !error_is_ignored();
}

void ODBCStatement::bind_in(int idx, const int& val)
{
    // cast away const because the ODBC API is not const-aware
    SQLBindParameter(stm, idx, SQL_PARAM_INPUT, get_odbc_integer_type<true, sizeof(const int)>(), SQL_INTEGER, 0, 0, (int*)&val, 0, 0);
}
void ODBCStatement::bind_in(int idx, const int& val, const SQLLEN& ind)
{
    // cast away const because the ODBC API is not const-aware
    SQLBindParameter(stm, idx, SQL_PARAM_INPUT, get_odbc_integer_type<true, sizeof(const int)>(), SQL_INTEGER, 0, 0, (int*)&val, 0, (SQLLEN*)&ind);
}

void ODBCStatement::bind_in(int idx, const unsigned& val)
{
    // cast away const because the ODBC API is not const-aware
    SQLBindParameter(stm, idx, SQL_PARAM_INPUT, get_odbc_integer_type<false, sizeof(const unsigned)>(), SQL_INTEGER, 0, 0, (unsigned*)&val, 0, 0);
}
void ODBCStatement::bind_in(int idx, const unsigned& val, const SQLLEN& ind)
{
    // cast away const because the ODBC API is not const-aware
    SQLBindParameter(stm, idx, SQL_PARAM_INPUT, get_odbc_integer_type<false, sizeof(const unsigned)>(), SQL_INTEGER, 0, 0, (unsigned*)&val, 0, (SQLLEN*)&ind);
}

void ODBCStatement::bind_in(int idx, const unsigned short& val)
{
    // cast away const because the ODBC API is not const-aware
    SQLBindParameter(stm, idx, SQL_PARAM_INPUT, get_odbc_integer_type<false, sizeof(const unsigned short)>(), SQL_INTEGER, 0, 0, (unsigned short*)&val, 0, 0);
}
void ODBCStatement::bind_in(int idx, const unsigned short& val, const SQLLEN& ind)
{
    // cast away const because the ODBC API is not const-aware
    SQLBindParameter(stm, idx, SQL_PARAM_INPUT, get_odbc_integer_type<false, sizeof(const unsigned short)>(), SQL_INTEGER, 0, 0, (unsigned short*)&val, 0, (SQLLEN*)&ind);
}

void ODBCStatement::bind_in(int idx, const char* val)
{
    // cast away const because the ODBC API is not const-aware
    SQLBindParameter(stm, idx, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_CHAR, 0, 0, (char*)val, 0, 0);
}
void ODBCStatement::bind_in(int idx, const char* val, const SQLLEN& ind)
{
    // cast away const because the ODBC API is not const-aware
    SQLBindParameter(stm, idx, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_CHAR, 0, 0, (char*)val, 0, (SQLLEN*)&ind);
}

void ODBCStatement::bind_in(int idx, const SQL_TIMESTAMP_STRUCT& val)
{
    // cast away const because the ODBC API is not const-aware
    //if (conn.server_type == POSTGRES || conn.server_type == SQLITE)
        SQLBindParameter(stm, idx, SQL_PARAM_INPUT, SQL_C_TYPE_TIMESTAMP, SQL_TIMESTAMP, 0, 0, (SQL_TIMESTAMP_STRUCT*)&val, 0, 0);
    //else
        //SQLBindParameter(stm, idx, SQL_PARAM_INPUT, SQL_C_TYPE_TIMESTAMP, SQL_DATETIME, 0, 0, (SQL_TIMESTAMP_STRUCT*)&val, 0, 0);
}

void ODBCStatement::bind_out(int idx, int& val)
{
    SQLBindCol(stm, idx, get_odbc_integer_type<true, sizeof(int)>(), &val, sizeof(val), 0);
}
void ODBCStatement::bind_out(int idx, int& val, SQLLEN& ind)
{
    SQLBindCol(stm, idx, get_odbc_integer_type<true, sizeof(int)>(), &val, sizeof(val), &ind);
}

void ODBCStatement::bind_out(int idx, unsigned& val)
{
    SQLBindCol(stm, idx, get_odbc_integer_type<false, sizeof(int)>(), &val, sizeof(val), 0);
}
void ODBCStatement::bind_out(int idx, unsigned& val, SQLLEN& ind)
{
    SQLBindCol(stm, idx, get_odbc_integer_type<false, sizeof(int)>(), &val, sizeof(val), &ind);
}

void ODBCStatement::bind_out(int idx, unsigned short& val)
{
    SQLBindCol(stm, idx, SQL_C_USHORT, &val, sizeof(val), 0);
}
void ODBCStatement::bind_out(int idx, unsigned short& val, SQLLEN& ind)
{
    SQLBindCol(stm, idx, SQL_C_USHORT, &val, sizeof(val), &ind);
}

void ODBCStatement::bind_out(int idx, char* val, SQLLEN buflen)
{
    SQLBindCol(stm, idx, SQL_C_CHAR, val, buflen, 0);
}
void ODBCStatement::bind_out(int idx, char* val, SQLLEN buflen, SQLLEN& ind)
{
    SQLBindCol(stm, idx, SQL_C_CHAR, val, buflen, &ind);
}

void ODBCStatement::bind_out(int idx, SQL_TIMESTAMP_STRUCT& val)
{
    SQLBindCol(stm, idx, SQL_C_TYPE_TIMESTAMP, &val, sizeof(val), 0);
}
void ODBCStatement::bind_out(int idx, SQL_TIMESTAMP_STRUCT& val, SQLLEN& ind)
{
    SQLBindCol(stm, idx, SQL_C_TYPE_TIMESTAMP, &val, sizeof(val), &ind);
}

bool ODBCStatement::fetch()
{
    int sqlres = SQLFetch(stm);
    if (sqlres == SQL_NO_DATA)
    {
#ifdef DEBUG_WARN_OPEN_TRANSACTIONS
        debug_reached_completion = true;
#endif
        return false;
    }
    if (is_error(sqlres))
        throw error_odbc(SQL_HANDLE_STMT, stm, "fetching data");
    return true;
}

bool ODBCStatement::fetch_expecting_one()
{
    if (!fetch())
    {
        close_cursor();
        return false;
    }
    if (fetch())
        throw error_consistency("expecting only one result from statement");
    close_cursor();
    return true;
}

size_t ODBCStatement::select_rowcount()
{
    if (conn.server_quirks & DBA_DB_QUIRK_NO_ROWCOUNT_IN_DIAG)
        return rowcount();

    SQLLEN res;
    int sqlres = SQLGetDiagField(SQL_HANDLE_STMT, stm, 0, SQL_DIAG_CURSOR_ROW_COUNT, &res, 0, NULL);
    // SQLRowCount is broken in newer sqlite odbc
    //int sqlres = SQLRowCount(stm, &res);
    if (is_error(sqlres))
        throw error_odbc(SQL_HANDLE_STMT, stm, "reading row count");
    return res;
}

size_t ODBCStatement::rowcount()
{
    SQLLEN res;
    int sqlres = SQLRowCount(stm, &res);
    if (is_error(sqlres))
        throw error_odbc(SQL_HANDLE_STMT, stm, "reading row count");
    return res;
}

void ODBCStatement::set_cursor_forward_only()
{
    int sqlres = SQLSetStmtAttr(stm, SQL_ATTR_CURSOR_TYPE,
        (SQLPOINTER)SQL_CURSOR_FORWARD_ONLY, SQL_IS_INTEGER);
    if (is_error(sqlres))
        throw error_odbc(SQL_HANDLE_STMT, stm, "setting SQL_CURSOR_FORWARD_ONLY");
}

void ODBCStatement::set_cursor_static()
{
    int sqlres = SQLSetStmtAttr(stm, SQL_ATTR_CURSOR_TYPE,
        (SQLPOINTER)SQL_CURSOR_STATIC, SQL_IS_INTEGER);
    if (is_error(sqlres))
        throw error_odbc(SQL_HANDLE_STMT, stm, "setting SQL_CURSOR_STATIC");
}

void ODBCStatement::close_cursor()
{
    int sqlres = SQLCloseCursor(stm);
    if (is_error(sqlres))
        throw error_odbc(SQL_HANDLE_STMT, stm, "closing cursor");
#ifdef DEBUG_WARN_OPEN_TRANSACTIONS
    debug_reached_completion = true;
#endif
}

void ODBCStatement::prepare(const char* query)
{
#ifdef DEBUG_WARN_OPEN_TRANSACTIONS
    if (!debug_reached_completion)
    {
        string msg = "Statement " + debug_query + " prepare was called before reaching completion";
        fprintf(stderr, "-- %s\n", msg.c_str());
        //throw error_consistency(msg);
    }
#endif
#ifdef DEBUG_WARN_OPEN_TRANSACTIONS
    debug_query = query;
#endif
    // Casting out 'const' because ODBC API is not const-conscious
    if (is_error(SQLPrepare(stm, (unsigned char*)query, SQL_NTS)))
        error_odbc::throwf(SQL_HANDLE_STMT, stm, "compiling query \"%s\"", query);
}

void ODBCStatement::prepare(const char* query, int qlen)
{
#ifdef DEBUG_WARN_OPEN_TRANSACTIONS
    debug_query = string(query, qlen);
#endif
    // Casting out 'const' because ODBC API is not const-conscious
    if (is_error(SQLPrepare(stm, (unsigned char*)query, qlen)))
        error_odbc::throwf(SQL_HANDLE_STMT, stm, "compiling query \"%.*s\"", qlen, query);
}

void ODBCStatement::prepare(const std::string& query)
{
    prepare(query.data(), query.size());
}

int ODBCStatement::execute()
{
#ifdef DEBUG_WARN_OPEN_TRANSACTIONS
    if (!debug_reached_completion)
    {
        string msg = "Statement " + debug_query + " restarted before reaching completion";
        fprintf(stderr, "-- %s\n", msg.c_str());
        //throw error_consistency(msg);
    }
#endif
    int sqlres = SQLExecute(stm);
    if (is_error(sqlres))
        throw error_odbc(SQL_HANDLE_STMT, stm, "executing precompiled query");
#ifdef DEBUG_WARN_OPEN_TRANSACTIONS
    debug_reached_completion = false;
#endif
    return sqlres;
}

int ODBCStatement::exec_direct(const char* query)
{
#ifdef DEBUG_WARN_OPEN_TRANSACTIONS
    debug_query = query;
#endif
    // Casting out 'const' because ODBC API is not const-conscious
    int sqlres = SQLExecDirect(stm, (SQLCHAR*)query, SQL_NTS);
    if (is_error(sqlres))
        error_odbc::throwf(SQL_HANDLE_STMT, stm, "executing query \"%s\"", query);
#ifdef DEBUG_WARN_OPEN_TRANSACTIONS
    debug_reached_completion = false;
#endif
    return sqlres;
}

int ODBCStatement::exec_direct(const char* query, int qlen)
{
#ifdef DEBUG_WARN_OPEN_TRANSACTIONS
    debug_query = string(query, qlen);
#endif
    // Casting out 'const' because ODBC API is not const-conscious
    int sqlres = SQLExecDirect(stm, (SQLCHAR*)query, qlen);
    if (is_error(sqlres))
        error_odbc::throwf(SQL_HANDLE_STMT, stm, "executing query \"%.*s\"", qlen, query);
#ifdef DEBUG_WARN_OPEN_TRANSACTIONS
    debug_reached_completion = false;
#endif
    return sqlres;
}

void ODBCStatement::close_cursor_if_needed()
{
    /*
    // If the query raised an error that we are ignoring, closing the cursor
    // would raise invalid cursor state
    if (sqlres != SQL_ERROR)
        close_cursor();
#ifdef DEBUG_WARN_OPEN_TRANSACTIONS
    else if (sqlres == SQL_ERROR && error_is_ignored())
        debug_reached_completion = true;
#endif
*/
    SQLCloseCursor(stm);
#ifdef DEBUG_WARN_OPEN_TRANSACTIONS
    debug_reached_completion = true;
#endif
}

void ODBCStatement::execute_ignoring_results()
{
    execute_and_close();
}

int ODBCStatement::execute_and_close()
{
    int sqlres = SQLExecute(stm);
    if (is_error(sqlres))
        throw error_odbc(SQL_HANDLE_STMT, stm, "executing precompiled query");
#ifdef DEBUG_WARN_OPEN_TRANSACTIONS
    debug_reached_completion = false;
#endif
    close_cursor_if_needed();
    return sqlres;
}

int ODBCStatement::exec_direct_and_close(const char* query)
{
#ifdef DEBUG_WARN_OPEN_TRANSACTIONS
    debug_query = query;
#endif
    // Casting out 'const' because ODBC API is not const-conscious
    int sqlres = SQLExecDirect(stm, (SQLCHAR*)query, SQL_NTS);
    if (is_error(sqlres))
        error_odbc::throwf(SQL_HANDLE_STMT, stm, "executing query \"%s\"", query);
#ifdef DEBUG_WARN_OPEN_TRANSACTIONS
    debug_reached_completion = false;
#endif
    close_cursor_if_needed();
    return sqlres;
}

int ODBCStatement::exec_direct_and_close(const char* query, int qlen)
{
#ifdef DEBUG_WARN_OPEN_TRANSACTIONS
    debug_query = string(query, qlen);
#endif
    // Casting out 'const' because ODBC API is not const-conscious
    int sqlres = SQLExecDirect(stm, (SQLCHAR*)query, qlen);
    if (is_error(sqlres))
        error_odbc::throwf(SQL_HANDLE_STMT, stm, "executing query \"%.*s\"", qlen, query);
#ifdef DEBUG_WARN_OPEN_TRANSACTIONS
    debug_reached_completion = false;
#endif
    close_cursor_if_needed();
    return sqlres;
}

int ODBCStatement::columns_count()
{
    SQLSMALLINT res;
    int sqlres = SQLNumResultCols(stm, &res);
    if (is_error(sqlres))
        throw error_odbc(SQL_HANDLE_STMT, stm, "querying number of columns in the result set");
    return res;
}

Sequence::Sequence(ODBCConnection& conn, const char* name)
    : ODBCStatement(conn)
{
    char qbuf[100];
    int qlen;

    bind_out(1, out);
    if (conn.server_type == ServerType::ORACLE)
        qlen = snprintf(qbuf, 100, "SELECT %s.CurrVal FROM dual", name);
    else
        qlen = snprintf(qbuf, 100, "SELECT last_value FROM %s", name);
    prepare(qbuf, qlen);
}

Sequence::~Sequence() {}

const int& Sequence::read()
{
    if (is_error(SQLExecute(stm)))
        throw error_odbc(SQL_HANDLE_STMT, stm, "reading sequence value");
    /* Get the result */
    if (SQLFetch(stm) == SQL_NO_DATA)
        throw error_notfound("fetching results of sequence value reads");
    if (is_error(SQLCloseCursor(stm)))
        throw error_odbc(SQL_HANDLE_STMT, stm, "closing sequence read cursor");
    return out;
}

std::ostream& operator<<(std::ostream& o, const SQL_TIMESTAMP_STRUCT& t)
{
    char buf[20];
    snprintf(buf, 20, "%04d-%02d-%02d %02d:%02d:%02d.%d", t.year, t.month, t.day, t.hour, t.minute, t.second, t.fraction);
    o << buf;
    return o;
}
#endif

}
}
