/*
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
#include "db/test-utils-db.h"
#include "internals.h"

using namespace dballe;
using namespace dballe::db;
using namespace wreport;
using namespace wibble::tests;
using namespace std;

namespace tut {

struct db_sqlite_internals_shar
{
    SQLiteConnection conn;

    db_sqlite_internals_shar()
    {
        conn.open_memory();
    }

    ~db_sqlite_internals_shar()
    {
    }

    void reset()
    {
        conn.drop_table_if_exists("dballe_test");
        conn.exec("CREATE TABLE dballe_test (val INTEGER NOT NULL)");
    }
};
TESTGRP(db_sqlite_internals);

// Test querying int values
template<> template<>
void to::test<1>()
{
    reset();

    conn.exec("INSERT INTO dballe_test VALUES (1)");
    conn.exec("INSERT INTO dballe_test VALUES (?)", 2);

    auto s = conn.sqlitestatement("SELECT val FROM dballe_test");

    int val = 0;
    unsigned count = 0;
    s->execute([&]() {
        val += s->column_int(0);
        ++count;
    });
    wassert(actual(count) == 2);
    wassert(actual(val) == 3);
}

#if 0
// Test querying int values, with indicators
template<> template<>
void to::test<2>()
{
    use_db();
    reset();

    auto& c = connection();
    auto s = c.sqlitestatement();

    s->exec_direct("INSERT INTO dballe_test VALUES (42)");

    s->prepare("SELECT val FROM dballe_test");
    int val = 0;
    SQLLEN ind = 0;
    s->bind_out(1, val, ind);
    s->execute();
    unsigned count = 0;
    while (s->fetch())
        ++count;
    wassert(actual(val) == 42);
    wassert(actual(ind) != SQL_NULL_DATA);
    wassert(actual(count) == 1);
}

// Test querying unsigned values
template<> template<>
void to::test<3>()
{
    use_db();
    reset();

    auto& c = connection();
    auto s = c.sqlitestatement();

    s->exec_direct("INSERT INTO dballe_test VALUES (42)");

    s->prepare("SELECT val FROM dballe_test");
    unsigned val = 0;
    s->bind_out(1, val);
    s->execute();
    unsigned count = 0;
    while (s->fetch())
        ++count;
    wassert(actual(val) == 42);
    wassert(actual(count) == 1);
}

// Test querying unsigned values, with indicators
template<> template<>
void to::test<4>()
{
    use_db();
    reset();

    auto& c = connection();
    auto s = c.sqlitestatement();

    s->exec_direct("INSERT INTO dballe_test VALUES (42)");

    s->prepare("SELECT val FROM dballe_test");
    unsigned val = 0;
    SQLLEN ind = 0;
    s->bind_out(1, val, ind);
    s->execute();
    unsigned count = 0;
    while (s->fetch())
        ++count;
    wassert(actual(val) == 42);
    wassert(actual(ind) != SQL_NULL_DATA);
    wassert(actual(count) == 1);
}

// Test querying unsigned short values
template<> template<>
void to::test<5>()
{
    use_db();
    reset();

    auto& c = connection();
    auto s = c.sqlitestatement();

    s->exec_direct("INSERT INTO dballe_test VALUES (42)");

    s->prepare("SELECT val FROM dballe_test");
    unsigned short val = 0;
    s->bind_out(1, val);
    s->execute();
    unsigned count = 0;
    while (s->fetch())
        ++count;
    wassert(actual(val) == 42);
    wassert(actual(count) == 1);
}

// Test has_tables
template<> template<>
void to::test<6>()
{
    use_db();
    reset();

    db::Connection& c = connection();
    ensure(!c.has_table("this_should_not_exist"));
    ensure(c.has_table("dballe_test"));
}

// Test settings
template<> template<>
void to::test<7>()
{
    use_db();
    auto& c = connection();
    c.drop_table_if_exists("dballe_settings");
    ensure(!c.has_table("dballe_settings"));

    ensure_equals(c.get_setting("test_key"), "");

    c.set_setting("test_key", "42");
    ensure(c.has_table("dballe_settings"));

    ensure_equals(c.get_setting("test_key"), "42");
}
#endif

}
