/*
 * db/sql/attrv6 - v6 implementation of attr table
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

#include "attrv6.h"
#include "internals.h"
#include "dballe/core/record.h"

using namespace wreport;
using namespace std;

namespace dballe {
namespace db {
namespace sql {

AttrV6::~AttrV6() {}

void AttrV6::add(int id_data, const Record& attrs)
{
    sql::AttributeList alist;
    for (vector<Var*>::const_iterator i = attrs.vars().begin(); i != attrs.vars().end(); ++i)
        if ((*i)->value() != NULL)
            alist.add((*i)->code(), (*i)->value());
    if (alist.empty()) return;
    impl_add(id_data, alist);
}

void AttrV6::add(int id_data, const wreport::Var& var)
{
    sql::AttributeList alist;
    for (const Var* attr = var.next_attr(); attr != NULL; attr = attr->next_attr())
        if (attr->value() != NULL)
            alist.add(attr->code(), attr->value());
    if (alist.empty()) return;
    impl_add(id_data, alist);
}

}
}
}
