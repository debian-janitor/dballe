/*
 * memdb/core - Core functions for memdb implementation
 *
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

#ifndef DBA_MEMDB_CORE_H
#define DBA_MEMDB_CORE_H

#include <set>
#include <map>
#include <cstddef>

namespace dballe {
namespace memdb {

/// Coordinates
struct Coord
{
    double lat;
    double lon;

    Coord() {}
    Coord(double lat, double lon) : lat(lat), lon(lon) {}

    bool operator<(const Coord& c) const
    {
        if (lat < c.lat) return true;
        if (lat > c.lat) return false;
        return lon < c.lon;
    }
};

/// Indices of elements inside a vector
struct Positions : public std::set<size_t>
{
    bool contains(size_t val) const
    {
        return find(val) != end();
    }

    void inplace_intersect(const Positions& pos)
    {
        // Inplace intersection
        typename Positions::iterator ia = begin();
        typename Positions::const_iterator ib = pos.begin();
        while (ia != end() && ib != pos.end())
        {
            if (*ia < *ib)
                erase(ia++);
            else if (*ib < *ia)
                ++ib;
            else
            {
                ++ia;
                ++ib;
            }
        }
        erase(ia, end());
    }
};

/// Index a vector's elements based by one value
template<typename T>
struct Index : public std::map<T, Positions>
{
    /// Lookup all positions for a value
    Positions search(const T& el) const
    {
        typename std::map<T, Positions>::const_iterator i = this->find(el);
        if (i == this->end())
            return Positions();
        else
            return i->second;
    }

    /// Refine a Positions set with the results of this lookup
    void refine(const T& el, Positions& res)
    {
        if (res.empty()) return;

        typename std::map<T, Positions>::const_iterator i = this->find(el);
        // If we have no results, the result set becomes empty
        if (i == this->end())
        {
            res.clear();
            return;
        }

        res.inplace_intersect(i->second);
    }
};

}
}

#endif
