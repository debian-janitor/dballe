#ifndef DBALLE_DB_V7_SQLITE_DATA_H
#define DBALLE_DB_V7_SQLITE_DATA_H

#include <dballe/db/v7/data.h>
#include <dballe/sql/fwd.h>

namespace dballe {
struct Record;

namespace db {
namespace v7 {
namespace sqlite {
struct DB;

/**
 * Precompiled query to manipulate the station data table
 */
class SQLiteStationData : public v7::StationData
{
protected:
    /// DB connection
    dballe::sql::SQLiteConnection& conn;

    /// Precompiled select statement
    dballe::sql::SQLiteStatement* sstm = nullptr;
    /// Precompiled insert statement
    dballe::sql::SQLiteStatement* istm = nullptr;
    /// Precompiled update statement
    dballe::sql::SQLiteStatement* ustm = nullptr;

    void _dump(std::function<void(int, int, wreport::Varcode, const char*)> out) override;

public:
    SQLiteStationData(dballe::sql::SQLiteConnection& conn);
    SQLiteStationData(const SQLiteStationData&) = delete;
    SQLiteStationData(const SQLiteStationData&&) = delete;
    SQLiteStationData& operator=(const SQLiteStationData&) = delete;
    ~SQLiteStationData();

    void insert(dballe::db::v7::Transaction& t, v7::bulk::InsertStationVars& vars, bulk::UpdateMode update_mode=bulk::UPDATE) override;
    void remove(const v7::QueryBuilder& qb) override;
};

/**
 * Precompiled query to manipulate the data table
 */
class SQLiteData : public v7::Data
{
protected:
    /// DB connection
    dballe::sql::SQLiteConnection& conn;

    /// Precompiled select statement
    dballe::sql::SQLiteStatement* sstm = nullptr;
    /// Precompiled insert statement
    dballe::sql::SQLiteStatement* istm = nullptr;
    /// Precompiled update statement
    dballe::sql::SQLiteStatement* ustm = nullptr;

    void _dump(std::function<void(int, int, int, const Datetime&, wreport::Varcode, const char*)> out) override;

public:
    SQLiteData(dballe::sql::SQLiteConnection& conn);
    SQLiteData(const SQLiteData&) = delete;
    SQLiteData(const SQLiteData&&) = delete;
    SQLiteData& operator=(const SQLiteData&) = delete;
    ~SQLiteData();

    void insert(dballe::db::v7::Transaction& t, v7::bulk::InsertVars& vars, bulk::UpdateMode update_mode=bulk::UPDATE) override;
    void remove(const v7::QueryBuilder& qb) override;
};

}
}
}
}
#endif