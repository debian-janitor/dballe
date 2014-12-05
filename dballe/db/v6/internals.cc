/*
 * db/v6/attr - attr table management
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

#include "internals.h"
#include "db.h"
#include "dballe/db/odbc/repinfo.h"
#include "dballe/db/odbc/v6_levtr.h"
#include "dballe/db/odbc/v6_data.h"
#include "dballe/db/odbc/v6_attr.h"
#include "dballe/core/record.h"
#include "dballe/msg/context.h"
#include "dballe/msg/msg.h"
#include <sstream>

using namespace wreport;
using namespace std;

namespace dballe {
namespace db {
namespace v6 {

std::unique_ptr<v5::Repinfo> create_repinfo(Connection& conn)
{
    if (ODBCConnection* c = dynamic_cast<ODBCConnection*>(&conn))
        return unique_ptr<v5::Repinfo>(new v6::ODBCRepinfo(*c));
    else
        throw error_unimplemented("v6 DB repinfo not yet implemented for non-ODBC connectors");
}

unique_ptr<LevTr> LevTr::create(DB& db)
{
    if (ODBCConnection* conn = dynamic_cast<ODBCConnection*>(db.conn))
        return unique_ptr<LevTr>(new ODBCLevTr(*conn));
    else
        throw error_unimplemented("v6 DB LevTr not yet implemented for non-ODBC connectors");
}

LevTr::~LevTr() {}


LevTrCache::~LevTrCache() {}

struct MapLevTrCache : public LevTrCache
{
    struct Item
    {
        int ltype1;
        int l1;
        int ltype2;
        int l2;
        int pind;
        int p1;
        int p2;
        Item(const LevTr::DBRow& o)
            : ltype1(o.ltype1), l1(o.l1),
              ltype2(o.ltype2), l2(o.l2),
              pind(o.pind), p1(o.p1), p2(o.p2) {}

        void to_record(Record& rec) const
        {
            if (ltype1 == MISSING_INT)
                rec.unset(DBA_KEY_LEVELTYPE1);
            else
                rec.set(DBA_KEY_LEVELTYPE1, ltype1);

            if (l1 == MISSING_INT)
                rec.unset(DBA_KEY_L1);
            else
                rec.set(DBA_KEY_L1, l1);

            if (ltype2 == MISSING_INT)
                rec.unset(DBA_KEY_LEVELTYPE2);
            else
                rec.set(DBA_KEY_LEVELTYPE2, ltype2);

            if (l2 == MISSING_INT)
                rec.unset(DBA_KEY_L2);
            else
                rec.set(DBA_KEY_L2, l2);

            if (pind == MISSING_INT)
                rec.unset(DBA_KEY_PINDICATOR);
            else
                rec.set(DBA_KEY_PINDICATOR, pind);

            if (p1 == MISSING_INT)
                rec.unset(DBA_KEY_P1);
            else
                rec.set(DBA_KEY_P1, p1);

            if (p2 == MISSING_INT)
                rec.unset(DBA_KEY_P2);
            else
                rec.set(DBA_KEY_P2, p2);
        }

        Level lev() const
        {
            return Level(ltype1, l1, ltype2, l2);
        }

        Trange tr() const
        {
            return Trange(pind, p1, p2);
        }
    };

    mutable LevTr* levtr;
    mutable map<int, Item> cache;

    MapLevTrCache(LevTr& levtr)
        : levtr(&levtr)
    {
        // TODO: make prefetch optional if needed, controlled by an env
        //       variable
        levtr.read_all([&](const LevTr::DBRow& row) {
            cache.insert(make_pair(row.id, Item(row)));
        });
    }

    const Item* get(int id) const
    {
        map<int, Item>::const_iterator i = cache.find(id);

        // Cache hit
        if (i != cache.end()) return &(i->second);

        // Miss: try the DB
        const LevTr::DBRow* row = levtr->read(id);
        if (!row) return 0;

        // Fill cache
        pair<map<int, Item>::iterator, bool> res = cache.insert(make_pair(id, Item(*row)));
        return &(res.first->second);
    }

    bool to_rec(int id, Record& rec)
    {
        const Item* i = get(id);
        if (!i) return 0;
        i->to_record(rec);
        return true;
    }

    Level to_level(int id) const
    {
        const Item* i = get(id);
        if (!i) return Level();
        return i->lev();
    }

    Trange to_trange(int id) const
    {
        const Item* i = get(id);
        if (!i) return Trange();
        return i->tr();
    }

    msg::Context* to_msg(int id, Msg& msg)
    {
        const Item* i = get(id);
        if (!i) return 0;
        msg::Context& res = msg.obtain_context(i->lev(), i->tr());
        return &res;
    }

    void invalidate()
    {
        cache.clear();
    }

    void dump(FILE* out) const
    {
        fprintf(out, "%zd elements in level/timerange cache:\n", cache.size());
        for (map<int, Item>::const_iterator i = cache.begin(); i != cache.end(); ++i)
        {
            stringstream str;
            str << i->second.lev();
            str << " ";
            str << i->second.tr();
            fprintf(out, "  %d: %s\n", i->first, str.str().c_str());
        }
    }
};


std::unique_ptr<LevTrCache> LevTrCache::create(DB& db)
{
    // TODO: check env vars to select alternate caching implementations, such
    // as a hit/miss that queries single items from the DB when not in cache
    if (ODBCConnection* conn = dynamic_cast<ODBCConnection*>(db.conn))
        return unique_ptr<LevTrCache>(new MapLevTrCache(db.lev_tr()));
    else
        throw error_unimplemented("v6 DB LevTr cache not yet implemented for non-ODBC connectors");
}

Data::~Data() {}

unique_ptr<Data> Data::create(DB& db)
{
    if (ODBCConnection* conn = dynamic_cast<ODBCConnection*>(db.conn))
        return unique_ptr<Data>(new ODBCData(db));
    else
        throw error_unimplemented("v6 DB Data not yet implemented for non-ODBC connectors");
}


Attr::~Attr() {}
unique_ptr<Attr> Attr::create(DB& db)
{
    if (ODBCConnection* conn = dynamic_cast<ODBCConnection*>(db.conn))
        return unique_ptr<Attr>(new ODBCAttr(*conn));
    else
        throw error_unimplemented("v6 DB attr not yet implemented for non-ODBC connectors");
}

}
}
}

