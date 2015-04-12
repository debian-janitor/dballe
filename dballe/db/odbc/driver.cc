/*
 * db/odbc/driver - Backend ODBC driver
 *
 * Copyright (C) 2014--2015  ARPA-SIM <urpsim@smr.arpa.emr.it>
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
#include "driver.h"
#include "repinfo.h"
#include "station.h"
#include "levtr.h"
#include "datav5.h"
#include "datav6.h"
#include "attrv5.h"
#include "attrv6.h"
#include "dballe/db/v6/qbuilder.h"
#include <sqltypes.h>
#include <sql.h>
#include <algorithm>
#include <cstring>

using namespace std;
using namespace wreport;

namespace dballe {
namespace db {
namespace odbc {

Driver::Driver(ODBCConnection& conn)
    : conn(conn)
{
}

Driver::~Driver()
{
}

std::unique_ptr<sql::Repinfo> Driver::create_repinfov5()
{
    return unique_ptr<sql::Repinfo>(new ODBCRepinfoV5(conn));
}

std::unique_ptr<sql::Repinfo> Driver::create_repinfov6()
{
    return unique_ptr<sql::Repinfo>(new ODBCRepinfoV6(conn));
}

std::unique_ptr<sql::Station> Driver::create_stationv5()
{
    return unique_ptr<sql::Station>(new ODBCStationV5(conn));
}

std::unique_ptr<sql::Station> Driver::create_stationv6()
{
    return unique_ptr<sql::Station>(new ODBCStationV6(conn));
}

std::unique_ptr<sql::LevTr> Driver::create_levtrv6()
{
    return unique_ptr<sql::LevTr>(new ODBCLevTrV6(conn));
}

std::unique_ptr<sql::DataV5> Driver::create_datav5()
{
    return unique_ptr<sql::DataV5>(new ODBCDataV5(conn));
}

std::unique_ptr<sql::DataV6> Driver::create_datav6()
{
    return unique_ptr<sql::DataV6>(new ODBCDataV6(conn));
}

std::unique_ptr<sql::AttrV5> Driver::create_attrv5()
{
    return unique_ptr<sql::AttrV5>(new ODBCAttrV5(conn));
}

std::unique_ptr<sql::AttrV6> Driver::create_attrv6()
{
    return unique_ptr<sql::AttrV6>(new ODBCAttrV6(conn));
}

void Driver::bulk_insert_v6(sql::bulk::InsertV6& vars, bool update_existing)
{
    std::sort(vars.begin(), vars.end());

    Querybuf select_query(512);
    select_query.appendf(R"(
        SELECT id, id_lev_tr, id_var, value
          FROM data
         WHERE id_station=%d AND id_report=%d AND datetime={ts '%04d-%02d-%02d %02d:%02d:%02d'}
         ORDER BY id_lev_tr, id_var
    )", vars.id_station, vars.id_report,
        vars.datetime.date.year, vars.datetime.date.month, vars.datetime.date.day,
        vars.datetime.time.hour, vars.datetime.time.minute, vars.datetime.time.second);

    // Get the current status of variables for this context
    auto stm = conn.odbcstatement(select_query);

    int id_data;
    int id_levtr;
    int id_var;
    char value[255];
    stm->bind_out(1, id_data);
    stm->bind_out(2, id_levtr);
    stm->bind_out(3, id_var);
    stm->bind_out(4, value, 255);

    // Scan the result in parallel with the variable list, annotating changed
    // items with their data ID
    sql::bulk::AnnotateVarsV6 todo(vars);
    stm->execute();
    while (stm->fetch())
        todo.annotate(id_data, id_levtr, id_var, value);
    todo.annotate_end();

    // We now have a todo-list

    if (update_existing && todo.do_update)
    {
        auto update_stm = conn.odbcstatement("UPDATE data SET value=? WHERE id=?");
        for (auto& v: vars)
        {
            if (!v.needs_update()) continue;
            update_stm->bind_in(1, v.var->value());
            update_stm->bind_in(2, v.id_data);
            update_stm->execute_and_close();
            v.set_updated();
        }
    }

    if (todo.do_insert)
    {
        Querybuf dq(512);
        dq.appendf(R"(
            INSERT INTO data (id_station, id_report, id_lev_tr, datetime, id_var, value)
                 VALUES (%d, %d, ?, '%04d-%02d-%02d %02d:%02d:%02d', ?, ?)
        )", vars.id_station, vars.id_report,
            vars.datetime.date.year, vars.datetime.date.month, vars.datetime.date.day,
            vars.datetime.time.hour, vars.datetime.time.minute, vars.datetime.time.second);
        auto insert = conn.odbcstatement(dq);
        int varcode;
        insert->bind_in(2, varcode);
        for (auto& v: vars)
        {
            if (!v.needs_insert()) continue;
            insert->bind_in(1, v.id_levtr);
            varcode = v.var->code();
            insert->bind_in(3, v.var->value());
            insert->execute();
            v.id_data = conn.get_last_insert_id();
            v.set_inserted();
        }
    }
}

void Driver::run_built_query_v6(
        const v6::QueryBuilder& qb,
        std::function<void(sql::SQLRecordV6& rec)> dest)
{
    auto stm = conn.odbcstatement(qb.sql_query);

    if (qb.bind_in_ident) stm->bind_in(1, qb.bind_in_ident);

    sql::SQLRecordV6 rec;
    int output_seq = 1;

    SQLLEN out_ident_ind;

    if (qb.select_station)
    {
        stm->bind_out(output_seq++, rec.out_ana_id);
        stm->bind_out(output_seq++, rec.out_lat);
        stm->bind_out(output_seq++, rec.out_lon);
        stm->bind_out(output_seq++, rec.out_ident, sizeof(rec.out_ident), out_ident_ind);
    }

    if (qb.select_varinfo)
    {
        stm->bind_out(output_seq++, rec.out_rep_cod);
        stm->bind_out(output_seq++, rec.out_id_ltr);
        stm->bind_out(output_seq++, rec.out_varcode);
    }

    if (qb.select_data_id)
        stm->bind_out(output_seq++, rec.out_id_data);

    SQL_TIMESTAMP_STRUCT out_datetime;
    if (qb.select_data)
    {
        stm->bind_out(output_seq++, out_datetime);
        stm->bind_out(output_seq++, rec.out_value, sizeof(rec.out_value));
    }

    SQL_TIMESTAMP_STRUCT out_datetimemax;
    if (qb.select_summary_details)
    {
        stm->bind_out(output_seq++, rec.out_id_data);
        stm->bind_out(output_seq++, out_datetime);
        stm->bind_out(output_seq++, out_datetimemax);
    }

    stm->execute();

    while (stm->fetch())
    {
        // Apply fixes here to demangle timestamps and NULL indicators
        if (out_ident_ind == SQL_NULL_DATA)
            rec.out_ident_size = -1;
        else
            rec.out_ident_size = out_ident_ind;

        rec.out_datetime.date.year = out_datetime.year;
        rec.out_datetime.date.month = out_datetime.month;
        rec.out_datetime.date.day = out_datetime.day;
        rec.out_datetime.time.hour = out_datetime.hour;
        rec.out_datetime.time.minute = out_datetime.minute;
        rec.out_datetime.time.second = out_datetime.second;

        if (qb.select_summary_details)
        {
            rec.out_datetimemax.date.year = out_datetimemax.year;
            rec.out_datetimemax.date.month = out_datetimemax.month;
            rec.out_datetimemax.date.day = out_datetimemax.day;
            rec.out_datetimemax.time.hour = out_datetimemax.hour;
            rec.out_datetimemax.time.minute = out_datetimemax.minute;
            rec.out_datetimemax.time.second = out_datetimemax.second;
        }

        dest(rec);
    }

    stm->close_cursor();
}

void Driver::run_delete_query_v6(const v6::QueryBuilder& qb)
{
    auto stm = conn.odbcstatement(qb.sql_query);
    if (qb.bind_in_ident) stm->bind_in(1, qb.bind_in_ident);

    // Get the list of data to delete
    int out_id_data;
    stm->bind_out(1, out_id_data);

    // Compile the DELETE query for the data
    auto stmd = conn.odbcstatement("DELETE FROM data WHERE id=?");
    stmd->bind_in(1, out_id_data);

    // Compile the DELETE query for the attributes
    auto stma = conn.odbcstatement("DELETE FROM attr WHERE id_data=?");
    stma->bind_in(1, out_id_data);

    stm->execute();

    // Iterate all the data_id results, deleting the related data and attributes
    while (stm->fetch())
    {
        stmd->execute_ignoring_results();
        stma->execute_ignoring_results();
    }
}

void Driver::create_tables_v5()
{
    switch (conn.server_type)
    {
        case ServerType::MYSQL:
            conn.exec(R"(
                CREATE TABLE station (
                   id         INTEGER auto_increment PRIMARY KEY,
                   lat        INTEGER NOT NULL,
                   lon        INTEGER NOT NULL,
                   ident      CHAR(64),
                   UNIQUE INDEX(lat, lon, ident(8)),
                   INDEX(lon)
                ) ENGINE=InnoDB;
            )");
            conn.exec(R"(
                CREATE TABLE repinfo (
                   id           SMALLINT PRIMARY KEY,
                   memo         VARCHAR(20) NOT NULL,
                   description  VARCHAR(255) NOT NULL,
                   prio         INTEGER NOT NULL,
                   descriptor   CHAR(6) NOT NULL,
                   tablea       INTEGER NOT NULL,
                   UNIQUE INDEX (prio),
                   UNIQUE INDEX (memo)
                ) ENGINE=InnoDB;
            )");
            conn.exec(R"(
                CREATE TABLE context (
                   id          INTEGER auto_increment PRIMARY KEY,
                   id_ana      INTEGER NOT NULL,
                   id_report   SMALLINT NOT NULL,
                   datetime    DATETIME NOT NULL,
                   ltype1      INTEGER NOT NULL,
                   l1          INTEGER NOT NULL,
                   ltype2      INTEGER NOT NULL,
                   l2          INTEGER NOT NULL,
                   ptype       INTEGER NOT NULL,
                   p1          INTEGER NOT NULL,
                   p2          INTEGER NOT NULL,
                   UNIQUE INDEX (id_ana, datetime, ltype1, l1, ltype2, l2, ptype, p1, p2, id_report),
                   INDEX (id_ana),
                   INDEX (id_report),
                   INDEX (datetime),
                   INDEX (ltype1, l1, ltype2, l2),
                   INDEX (ptype, p1, p2)
               ) ENGINE=InnoDB;
            )");
            conn.exec(R"(
                CREATE TABLE data (
                   id_context  INTEGER NOT NULL,
                   id_var      SMALLINT NOT NULL,
                   value       VARCHAR(255) NOT NULL,
                   INDEX (id_context),
                   UNIQUE INDEX(id_var, id_context)
               ) ENGINE=InnoDB;
            )");
            conn.exec(R"(
                CREATE TABLE attr (
                   id_context  INTEGER NOT NULL,
                   id_var      SMALLINT NOT NULL,
                   type        SMALLINT NOT NULL,
                   value       VARCHAR(255) NOT NULL,
                   INDEX (id_context, id_var),
                   UNIQUE INDEX (id_context, id_var, type)
               ) ENGINE=InnoDB;
            )");
            break;
        case ServerType::ORACLE:
            conn.exec(R"(
                CREATE TABLE station (
                   id         INTEGER PRIMARY KEY,
                   lat        INTEGER NOT NULL,
                   lon        INTEGER NOT NULL,
                   ident      VARCHAR2(64),
                   UNIQUE (lat, lon, ident)
                );
                CREATE INDEX pa_lon ON station(lon);
                CREATE SEQUENCE seq_station;
            )");
            conn.exec(R"(
                CREATE TABLE repinfo (
                   id           INTEGER PRIMARY KEY,
                   memo         VARCHAR2(30) NOT NULL,
                   description  VARCHAR2(255) NOT NULL,
                   prio         INTEGER NOT NULL,
                   descriptor   CHAR(6) NOT NULL,
                   tablea       INTEGER NOT NULL,
                   UNIQUE (prio),
                   UNIQUE (memo)
                )
            )");
            conn.exec(R"(
                CREATE TABLE context (
                   id          INTEGER PRIMARY KEY,
                   id_ana      INTEGER NOT NULL,
                   id_report   INTEGER NOT NULL,
                   datetime    DATE NOT NULL,
                   ltype1      INTEGER NOT NULL,
                   l1          INTEGER NOT NULL,
                   ltype2      INTEGER NOT NULL,
                   l2          INTEGER NOT NULL,
                   ptype       INTEGER NOT NULL,
                   p1          INTEGER NOT NULL,
                   p2          INTEGER NOT NULL,
                   UNIQUE (id_ana, datetime, ltype1, l1, ltype2, l2, ptype, p1, p2, id_report)
                );
                CREATE INDEX co_ana ON context(id_ana);
                CREATE INDEX co_report ON context(id_report);
                CREATE INDEX co_dt ON context(datetime);
                CREATE INDEX co_lt ON context(ltype1, l1, ltype2, l2);
                CREATE INDEX co_pt ON context(ptype, p1, p2);
                CREATE SEQUENCE seq_context;
            )");
            conn.exec(R"(
                CREATE TABLE data (
                   id_context  INTEGER NOT NULL,
                   id_var      INTEGER NOT NULL,
                   value       VARCHAR2(255) NOT NULL,
                   UNIQUE (id_var, id_context)
                );
                CREATE INDEX da_co ON data(id_context);
            )");
            conn.exec(R"(
                CREATE TABLE attr (
                   id_context  INTEGER NOT NULL,
                   id_var      INTEGER NOT NULL,
                   type        INTEGER NOT NULL,
                   value       VARCHAR2(255) NOT NULL,
                   UNIQUE (id_context, id_var, type)
                );
                CREATE INDEX at_da ON attr(id_context, id_var);
            )");
            break;
        default:
            throw error_unimplemented("ODBC connection is only supported for MySQL and Oracle");
    }
    conn.set_setting("version", "V5");
}
void Driver::create_tables_v6()
{
    switch (conn.server_type)
    {
        case ServerType::MYSQL:
            conn.exec(R"(
                CREATE TABLE station (
                   id         INTEGER auto_increment PRIMARY KEY,
                   lat        INTEGER NOT NULL,
                   lon        INTEGER NOT NULL,
                   ident      CHAR(64),
                   UNIQUE INDEX(lat, lon, ident(8)),
                   INDEX(lon)
                ) ENGINE=InnoDB;
            )");
            conn.exec(R"(
                CREATE TABLE repinfo (
                   id           SMALLINT PRIMARY KEY,
                   memo         VARCHAR(20) NOT NULL,
                   description  VARCHAR(255) NOT NULL,
                   prio         INTEGER NOT NULL,
                   descriptor   CHAR(6) NOT NULL,
                   tablea       INTEGER NOT NULL,
                   UNIQUE INDEX (prio),
                   UNIQUE INDEX (memo)
                ) ENGINE=InnoDB;
            )");
            conn.exec(R"(
                CREATE TABLE lev_tr (
                   id          INTEGER auto_increment PRIMARY KEY,
                   ltype1      INTEGER NOT NULL,
                   l1          INTEGER NOT NULL,
                   ltype2      INTEGER NOT NULL,
                   l2          INTEGER NOT NULL,
                   ptype       INTEGER NOT NULL,
                   p1          INTEGER NOT NULL,
                   p2          INTEGER NOT NULL,
                   UNIQUE INDEX (ltype1, l1, ltype2, l2, ptype, p1, p2)
               ) ENGINE=InnoDB;
            )");
            conn.exec(R"(
                CREATE TABLE data (
                   id          INTEGER auto_increment PRIMARY KEY,
                   id_station  SMALLINT NOT NULL,
                   id_report   INTEGER NOT NULL,
                   id_lev_tr   INTEGER NOT NULL,
                   datetime    DATETIME NOT NULL,
                   id_var      SMALLINT NOT NULL,
                   value       VARCHAR(255) NOT NULL,
                   UNIQUE INDEX(id_station, datetime, id_lev_tr, id_report, id_var),
                   INDEX(datetime),
                   INDEX(id_lev_tr)
               ) ENGINE=InnoDB;
            )");
            conn.exec(R"(
                CREATE TABLE attr (
                   id_data     INTEGER NOT NULL,
                   type        SMALLINT NOT NULL,
                   value       VARCHAR(255) NOT NULL,
                   UNIQUE INDEX (id_data, type)
               ) ENGINE=InnoDB;
            )");
            break;
        case ServerType::ORACLE:
            conn.exec(R"(
                CREATE TABLE station (
                   id         INTEGER PRIMARY KEY,
                   lat        INTEGER NOT NULL,
                   lon        INTEGER NOT NULL,
                   ident      VARCHAR2(64),
                   UNIQUE (lat, lon, ident)
                );
                CREATE INDEX pa_lon ON station(lon);
                CREATE SEQUENCE seq_station;
            )");
            conn.exec(R"(
                CREATE TABLE repinfo (
                   id           INTEGER PRIMARY KEY,
                   memo         VARCHAR2(30) NOT NULL,
                   description  VARCHAR2(255) NOT NULL,
                   prio         INTEGER NOT NULL,
                   descriptor   CHAR(6) NOT NULL,
                   tablea       INTEGER NOT NULL,
                   UNIQUE (prio),
                   UNIQUE (memo)
                )
            )");
            conn.exec(R"(
                CREATE TABLE lev_tr (
                   id          INTEGER PRIMARY KEY,
                   ltype1      INTEGER NOT NULL,
                   l1          INTEGER NOT NULL,
                   ltype2      INTEGER NOT NULL,
                   l2          INTEGER NOT NULL,
                   ptype       INTEGER NOT NULL,
                   p1          INTEGER NOT NULL,
                   p2          INTEGER NOT NULL,
                );
                CREATE SEQUENCE seq_lev_tr;
                CREATE UNIQUE INDEX lev_tr_uniq ON lev_tr(ltype1, l1, ltype2, l2, ptype, p1, p2);
            )");
            conn.exec(R"(
                CREATE TABLE data (
                   id          SERIAL PRIMARY KEY,
                   id_station  INTEGER NOT NULL,
                   id_report   INTEGER NOT NULL,
                   id_lev_tr   INTEGER NOT NULL,
                   datetime    DATE NOT NULL,
                   id_var      INTEGER NOT NULL,
                   value       VARCHAR(255) NOT NULL,
                );
                CREATE UNIQUE INDEX data_uniq(id_station, datetime, id_lev_tr, id_report, id_var);
                CREATE INDEX data_sta ON data(id_station);
                CREATE INDEX data_rep ON data(id_report);
                CREATE INDEX data_dt ON data(datetime);
                CREATE INDEX data_lt ON data(id_lev_tr);
            )");
            conn.exec(R"(
                CREATE TABLE attr ("
                   id_data     INTEGER NOT NULL,
                   type        INTEGER NOT NULL,
                   value       VARCHAR(255) NOT NULL,
                );
                CREATE UNIQUE INDEX attr_uniq ON attr(id_data, type);
            )");
            break;
        default:
            throw error_unimplemented("ODBC connection is only supported for MySQL and Oracle");
    }
    conn.set_setting("version", "V6");
}
void Driver::delete_tables_v5()
{
    conn.drop_sequence_if_exists("seq_context"); // Oracle only
    conn.drop_sequence_if_exists("seq_station"); // Oracle only
    conn.drop_table_if_exists("attr");
    conn.drop_table_if_exists("data");
    conn.drop_table_if_exists("context");
    conn.drop_table_if_exists("repinfo");
    conn.drop_table_if_exists("station");
    conn.drop_settings();
}
void Driver::delete_tables_v6()
{
    conn.drop_sequence_if_exists("seq_lev_tr");  // Oracle only
    conn.drop_sequence_if_exists("seq_station"); // Oracle only
    conn.drop_table_if_exists("attr");
    conn.drop_table_if_exists("data");
    conn.drop_table_if_exists("lev_tr");
    conn.drop_table_if_exists("repinfo");
    conn.drop_table_if_exists("station");
    conn.drop_settings();
}

}
}
}
