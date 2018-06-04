#ifndef DBALLE_DB_V7_STATION_H
#define DBALLE_DB_V7_STATION_H

#include <dballe/sql/fwd.h>
#include <dballe/db/v7/cache.h>
#include <memory>
#include <cstdio>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace wreport {
struct Var;
}

namespace dballe {
struct Record;
struct Coords;
struct Station;

namespace db {
namespace v7 {
struct Transaction;

struct Station
{
protected:
    virtual void _dump(std::function<void(int, int, const Coords& coords, const char* ident)> out) = 0;

public:
    virtual ~Station();

    /**
     * Get the station ID given latitude, longitude and mobile identifier.
     *
     * It returns MISSING_INT if it does not exist.
     */
    virtual int maybe_get_id(v7::Transaction& tr, const dballe::Station& st) = 0;

    /**
     * Insert a new station in the database, without checking if it already exists.
     *
     * Returns the ID of the new station
     */
    virtual int insert_new(v7::Transaction& tr, const dballe::Station& desc) = 0;

    /**
     * Dump the entire contents of the table to an output stream
     */
    void dump(FILE* out);

    /**
     * Export station variables
     */
    virtual void get_station_vars(int id_station, std::function<void(std::unique_ptr<wreport::Var>)> dest) = 0;

    /**
     * Add all station variables (without attributes) to rec.
     *
     * If the same variable exists in many different networks, the one with the
     * highest priority will be used.
     */
    virtual void add_station_vars(int id_station, Record& rec) = 0;
};

}
}
}
#endif

