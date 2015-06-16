/*
 * Copyright (C) 2015  ARPA-SIM <urpsim@smr.arpa.emr.it>
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
#include "db/tests.h"
#include "internals.h"

using namespace dballe;
using namespace dballe::db;
using namespace wreport;
using namespace wibble::tests;
using namespace std;

namespace tut {

struct db_mysql_internals_shar
{
    MySQLConnection conn;

    db_mysql_internals_shar()
    {
        conn.open_test();
    }

    ~db_mysql_internals_shar()
    {
    }

    void reset()
    {
        conn.drop_table_if_exists("dballe_test");
        conn.exec_no_data("CREATE TABLE dballe_test (val INTEGER NOT NULL)");
    }
};
TESTGRP(db_mysql_internals);

// Test parsing urls
template<> template<>
void to::test<1>()
{
    mysql::ConnectInfo info;

    info.parse_url("mysql:");
    wassert(actual(info.host) == "");
    wassert(actual(info.user) == "");
    wassert(actual(info.has_passwd).isfalse());
    wassert(actual(info.passwd) == "");
    wassert(actual(info.has_dbname).isfalse());
    wassert(actual(info.dbname) == "");
    wassert(actual(info.port) == 0);
    wassert(actual(info.unix_socket) == "");

    info.parse_url("mysql://");
    wassert(actual(info.host) == "");
    wassert(actual(info.user) == "");
    wassert(actual(info.has_passwd).isfalse());
    wassert(actual(info.passwd) == "");
    wassert(actual(info.has_dbname).isfalse());
    wassert(actual(info.dbname) == "");
    wassert(actual(info.port) == 0);
    wassert(actual(info.unix_socket) == "");

    info.parse_url("mysql://localhost/");
    wassert(actual(info.host) == "localhost");
    wassert(actual(info.user) == "");
    wassert(actual(info.has_passwd).isfalse());
    wassert(actual(info.passwd) == "");
    wassert(actual(info.has_dbname).isfalse());
    wassert(actual(info.dbname) == "");
    wassert(actual(info.port) == 0);
    wassert(actual(info.unix_socket) == "");

    info.parse_url("mysql://localhost:1234/");
    wassert(actual(info.host) == "localhost");
    wassert(actual(info.user) == "");
    wassert(actual(info.has_passwd).isfalse());
    wassert(actual(info.passwd) == "");
    wassert(actual(info.has_dbname).isfalse());
    wassert(actual(info.dbname) == "");
    wassert(actual(info.port) == 1234);
    wassert(actual(info.unix_socket) == "");

    info.parse_url("mysql://localhost:1234/?user=enrico");
    wassert(actual(info.host) == "localhost");
    wassert(actual(info.user) == "enrico");
    wassert(actual(info.has_passwd).isfalse());
    wassert(actual(info.passwd) == "");
    wassert(actual(info.has_dbname).isfalse());
    wassert(actual(info.dbname) == "");
    wassert(actual(info.port) == 1234);
    wassert(actual(info.unix_socket) == "");

    info.parse_url("mysql://localhost:1234/foo?user=enrico");
    wassert(actual(info.host) == "localhost");
    wassert(actual(info.user) == "enrico");
    wassert(actual(info.has_passwd).isfalse());
    wassert(actual(info.passwd) == "");
    wassert(actual(info.has_dbname).istrue());
    wassert(actual(info.dbname) == "foo");
    wassert(actual(info.port) == 1234);
    wassert(actual(info.unix_socket) == "");

    info.parse_url("mysql://localhost:1234/foo?user=enrico&password=secret");
    wassert(actual(info.host) == "localhost");
    wassert(actual(info.user) == "enrico");
    wassert(actual(info.has_passwd).istrue());
    wassert(actual(info.passwd) == "secret");
    wassert(actual(info.has_dbname).istrue());
    wassert(actual(info.dbname) == "foo");
    wassert(actual(info.port) == 1234);
    wassert(actual(info.unix_socket) == "");

    info.parse_url("mysql://localhost/foo?user=enrico&password=secret");
    wassert(actual(info.host) == "localhost");
    wassert(actual(info.user) == "enrico");
    wassert(actual(info.has_passwd).istrue());
    wassert(actual(info.passwd) == "secret");
    wassert(actual(info.has_dbname).istrue());
    wassert(actual(info.dbname) == "foo");
    wassert(actual(info.port) == 0);
    wassert(actual(info.unix_socket) == "");

    info.parse_url("mysql:///foo?user=enrico&password=secret");
    wassert(actual(info.host) == "");
    wassert(actual(info.user) == "enrico");
    wassert(actual(info.has_passwd).istrue());
    wassert(actual(info.passwd) == "secret");
    wassert(actual(info.has_dbname).istrue());
    wassert(actual(info.dbname) == "foo");
    wassert(actual(info.port) == 0);
    wassert(actual(info.unix_socket) == "");
}

// Test querying int values
template<> template<>
void to::test<2>()
{
    using namespace mysql;

    reset();

    conn.exec_no_data("INSERT INTO dballe_test VALUES (1)");
    conn.exec_no_data("INSERT INTO dballe_test VALUES (2)");

    auto res = conn.exec_store("SELECT val FROM dballe_test");

    int val = 0;
    unsigned count = 0;
    while (Row row = res.fetch())
    {
        val += row.as_int(0);
        ++count;
    }
    wassert(actual(count) == 2);
    wassert(actual(val) == 3);
}

// Test querying int values, with potential NULLs
template<> template<>
void to::test<3>()
{
    reset();

    conn.drop_table_if_exists("dballe_testnull");
    conn.exec_no_data("CREATE TABLE dballe_testnull (val INTEGER)");
    conn.exec_no_data("INSERT INTO dballe_testnull VALUES (NULL)");
    conn.exec_no_data("INSERT INTO dballe_testnull VALUES (42)");

    auto res = conn.exec_store("SELECT val FROM dballe_testnull");
    wassert(actual(res.rowcount()) == 2);

    int val = 0;
    unsigned count = 0;
    unsigned countnulls = 0;
    while (auto row = res.fetch())
    {
        if (row.isnull(0))
            ++countnulls;
        else
            val += row.as_int(0);
        ++count;
    }

    wassert(actual(val) == 42);
    wassert(actual(count) == 2);
    wassert(actual(countnulls) == 1);
}

// Test querying unsigned values
template<> template<>
void to::test<4>()
{
    reset();

    conn.drop_table_if_exists("dballe_testbig");
    conn.exec_no_data("CREATE TABLE dballe_testbig (val BIGINT)");
    conn.exec_no_data("INSERT INTO dballe_testbig VALUES (0xFFFFFFFE)");

    auto res = conn.exec_store("SELECT val FROM dballe_testbig");
    wassert(actual(res.rowcount()) == 1);

    unsigned val = 0;
    unsigned count = 0;
    while (auto row = res.fetch())
    {
        val += row.as_unsigned(0);
        ++count;
    }
    wassert(actual(val) == 0xFFFFFFFE);
    wassert(actual(count) == 1);
}

// Test querying unsigned short values
template<> template<>
void to::test<5>()
{
    reset();

    char buf[200];
    snprintf(buf, 200, "INSERT INTO dballe_test VALUES (%d)", (int)WR_VAR(3, 1, 12));
    conn.exec_no_data(buf);

    auto res = conn.exec_store("SELECT val FROM dballe_test");
    wassert(actual(res.rowcount()) == 1);

    Varcode val = 0;
    unsigned count = 0;
    while (auto row = res.fetch())
    {
        val = (Varcode)row.as_int(0);
        ++count;
    }
    wassert(actual(count) == 1);
    wassert(actual(val) == WR_VAR(3, 1, 12));
}

// Test has_tables
template<> template<>
void to::test<6>()
{
    reset();

    wassert(actual(conn.has_table("this_should_not_exist")).isfalse());
    wassert(actual(conn.has_table("dballe_test")).istrue());
}

// Test settings
template<> template<>
void to::test<7>()
{
    conn.drop_table_if_exists("dballe_settings");
    wassert(actual(conn.has_table("dballe_settings")).isfalse());

    wassert(actual(conn.get_setting("test_key")) == "");

    conn.set_setting("test_key", "42");
    wassert(actual(conn.has_table("dballe_settings")).istrue());

    wassert(actual(conn.get_setting("test_key")) == "42");
}

// Test auto_increment
template<> template<>
void to::test<8>()
{
    conn.drop_table_if_exists("dballe_testai");
    conn.exec_no_data("CREATE TABLE dballe_testai (id INTEGER AUTO_INCREMENT PRIMARY KEY, val INTEGER)");
    conn.exec_no_data("INSERT INTO dballe_testai (val) VALUES (42)");
    wassert(actual(conn.get_last_insert_id()) == 1);
    conn.exec_no_data("INSERT INTO dballe_testai (val) VALUES (43)");
    wassert(actual(conn.get_last_insert_id()) == 2);
}

}
