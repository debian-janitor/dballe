/*
 * Copyright (C) 2013  ARPA-SIM <urpsim@smr.arpa.emr.it>
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

#include <dballe/core/test-utils-core.h>
#include <dballe/memdb/query.h>
#include <vector>
#include <iterator>

namespace dballe {
namespace tests {

template<typename T>
static inline std::vector<const T*> _get_results(WIBBLE_TEST_LOCPRM, memdb::Results<T>& res)
{
    using namespace wibble::tests;

    wassert(actual(res.is_select_all()).isfalse());
    wassert(actual(res.is_empty()).isfalse());
    std::vector<const T*> items;
    res.copy_valptrs_to(std::back_inserter(items));
    return items;
}

#define get_results(res) _get_results(wibble_test_location.nest(wibble_test_location_info, __FILE__, __LINE__, "get_results(" #res ")"), res)

}
}

// vim:set ts=4 sw=4:
