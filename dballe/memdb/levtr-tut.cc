#include "memdb/tests.h"
#include "levtr.h"
#include "results.h"

using namespace dballe;
using namespace dballe::tests;
using namespace dballe::memdb;
using namespace wibble::tests;
using namespace std;

namespace tut {

struct memdb_levtr_shar
{
};

TESTGRP(memdb_levtr);

template<> template<> void to::test<1>()
{
    LevTrs values;

    // Insert a levtr and check that all data is there
    const LevTr& val = *values[values.obtain(Level(1), Trange::instant())];
    wassert(actual(val.level) == Level(1));
    wassert(actual(val.trange) == Trange::instant());

    // Check that lookup returns the same element
    const LevTr& val1 = *values[values.obtain(Level(1), Trange::instant())];
    wassert(actual(&val1) == &val);

    // Check again, looking up records
    core::Record rec;
    rec.set(Level(1));
    rec.set(Trange::instant());
    const LevTr& val2 = *values[values.obtain(rec)];
    wassert(actual(&val2) == &val);
}

template<> template<> void to::test<2>()
{
    LevTrs values;
    values.obtain(Level(1), Trange::instant());
    values.obtain(Level::cloud(2, 3), Trange(1, 2, 3));

    {
        Results<LevTr> res(values);
        values.query(core_query_from_string("leveltype1=1"), res);
        vector<const LevTr*> items = get_results(res);
        wassert(actual(items.size()) == 1);
        wassert(actual(items[0]->level) == Level(1));
    }

    {
        Results<LevTr> res(values);
        values.query(core_query_from_string("leveltype2=2"), res);
        vector<const LevTr*> items = get_results(res);
        wassert(actual(items.size()) == 1);
        wassert(actual(items[0]->level) == Level::cloud(2, 3));
    }

    {
        Results<LevTr> res(values);
        values.query(core_query_from_string("l2=3"), res);
        vector<const LevTr*> items = get_results(res);
        wassert(actual(items.size()) == 1);
        wassert(actual(items[0]->level) == Level::cloud(2, 3));
    }

    {
        Results<LevTr> res(values);
        values.query(core_query_from_string("pindicator=1"), res);
        vector<const LevTr*> items = get_results(res);
        wassert(actual(items.size()) == 1);
        wassert(actual(items[0]->level) == Level::cloud(2, 3));
    }

    {
        Results<LevTr> res(values);
        values.query(core_query_from_string("p1=2"), res);
        vector<const LevTr*> items = get_results(res);
        wassert(actual(items.size()) == 1);
        wassert(actual(items[0]->level) == Level::cloud(2, 3));
    }

    {
        Results<LevTr> res(values);
        values.query(core_query_from_string("p2=3"), res);
        vector<const LevTr*> items = get_results(res);
        wassert(actual(items.size()) == 1);
        wassert(actual(items[0]->level) == Level::cloud(2, 3));
    }
}

}

#include "results.tcc"
