/*
 * db/odbc/attrv5 - attr table management
 *
 * Copyright (C) 2005--2015  ARPA-SIM <urpsim@smr.arpa.emr.it>
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

#include "attrv5.h"
#include "dballe/db/odbc/internals.h"
#include <dballe/core/var.h>

#include <sql.h>
#include <cstring>

using namespace wreport;
using namespace std;

namespace dballe {
namespace db {
namespace odbc {

ODBCAttrV5::ODBCAttrV5(ODBCConnection& conn)
    : conn(conn), sstm(0), istm(0), rstm(0)
{
    const char* select_query =
        "SELECT type, value FROM attr WHERE id_context=? and id_var=?";
    const char* insert_query =
        "INSERT INTO attr (id_context, id_var, type, value)"
        " VALUES(?, ?, ?, ?)";
    const char* replace_query_mysql =
        "INSERT INTO attr (id_context, id_var, type, value)"
        " VALUES(?, ?, ?, ?) ON DUPLICATE KEY UPDATE value=VALUES(value)";
    const char* replace_query_sqlite =
        "INSERT OR REPLACE INTO attr (id_context, id_var, type, value)"
        " VALUES(?, ?, ?, ?)";
    const char* replace_query_oracle =
        "MERGE INTO attr USING"
        " (SELECT ? as cnt, ? as var, ? as t, ? as val FROM dual)"
        " ON (id_context=cnt AND id_var=var AND type=t)"
        " WHEN MATCHED THEN UPDATE SET value=val"
        " WHEN NOT MATCHED THEN"
        "  INSERT (id_context, id_var, type, value) VALUES (cnt, var, t, val)";
    const char* replace_query_postgres =
        "UPDATE attr SET value=? WHERE id_context=? AND id_var=? AND type=?";

    // Create the statement for select
    sstm = conn.odbcstatement(select_query).release();
    sstm->bind_in(1, id_context);
    sstm->bind_in(2, id_var);
    sstm->bind_out(1, type);
    sstm->bind_out(2, value, sizeof(value));

    // Create the statement for insert
    istm = conn.odbcstatement(insert_query).release();
    istm->bind_in(1, id_context);
    istm->bind_in(2, id_var);
    istm->bind_in(3, type);
    istm->bind_in(4, value, value_ind);

    // Create the statement for replace
    switch (conn.server_type)
    {
        case ServerType::MYSQL: rstm = conn.odbcstatement(replace_query_mysql).release(); break;
        case ServerType::SQLITE: rstm = conn.odbcstatement(replace_query_sqlite).release(); break;
        case ServerType::ORACLE: rstm = conn.odbcstatement(replace_query_oracle).release(); break;
        case ServerType::POSTGRES: rstm = conn.odbcstatement(replace_query_postgres).release(); break;
        default: rstm = conn.odbcstatement(replace_query_mysql).release(); break;
    }
    if (conn.server_type == ServerType::POSTGRES)
    {
        rstm->bind_in(1, value, value_ind);
        rstm->bind_in(2, id_context);
        rstm->bind_in(3, id_var);
        rstm->bind_in(4, type);
    } else {
        rstm->bind_in(1, id_context);
        rstm->bind_in(2, id_var);
        rstm->bind_in(3, type);
        rstm->bind_in(4, value, value_ind);
    }
}

ODBCAttrV5::~ODBCAttrV5()
{
    if (sstm) delete sstm;
    if (istm) delete istm;
    if (rstm) delete rstm;
}

void ODBCAttrV5::set_context(int id_context, wreport::Varcode id_var)
{
    this->id_context = id_context;
    this->id_var = id_var;
}

void ODBCAttrV5::set(const wreport::Var& var)
{
    type = var.code();
    set_value(var.value());
}

void ODBCAttrV5::set_value(const char* qvalue)
{
    if (qvalue == NULL)
    {
        value[0] = 0;
        value_ind = SQL_NULL_DATA;
    } else {
        int len = strlen(qvalue);
        if (len > 255) len = 255;
        memcpy(value, qvalue, len);
        value[len] = 0;
        value_ind = len;
    }
}

void ODBCAttrV5::insert()
{
    if (conn.server_type == ServerType::POSTGRES)
    {
        if (rstm->execute_and_close() == SQL_NO_DATA)
            istm->execute_and_close();
    } else
        rstm->execute_and_close();
}

void ODBCAttrV5::load(int id_context, wreport::Var& var)
{
    this->id_context = id_context;
    // Query all attributes for this var in the current context
    id_var = var.code();
    sstm->execute();

    // Make attribues from the result, and add them to var
    while (sstm->fetch())
        var.seta(ap_newvar(type, value));

    sstm->close_cursor();
}

void ODBCAttrV5::dump(FILE* out)
{
    int id_context;
    wreport::Varcode id_var;
    wreport::Varcode type;
    char value[255];
    SQLLEN value_ind;

    auto stm = conn.odbcstatement("SELECT id_context, id_var, type, value FROM attr");
    stm->bind_out(1, id_context);
    stm->bind_out(2, id_var);
    stm->bind_out(3, type);
    stm->bind_out(4, value, 255, value_ind);
    stm->execute();
    int count;
    fprintf(out, "dump of table attr:\n");
    for (count = 0; stm->fetch(); ++count)
    {
        fprintf(out, " %4d, %01d%02d%03d, %01d%02d%03d",
                (int)id_context,
                WR_VAR_F(id_var), WR_VAR_X(id_var), WR_VAR_Y(id_var),
                WR_VAR_F(type), WR_VAR_X(type), WR_VAR_Y(type));
        if (value_ind == SQL_NTS)
                fprintf(out, "\n");
        else
                fprintf(out, " %.*s\n", (int)value_ind, value);
    }
    fprintf(out, "%d element%s in table attr\n", count, count != 1 ? "s" : "");
    stm->close_cursor();
}

}
}
}
