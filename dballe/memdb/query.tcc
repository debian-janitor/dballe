/*
 * memdb/query - Infrastructure used to query memdb data
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

#ifndef DBA_MEMDB_QUERY_TCC
#define DBA_MEMDB_QUERY_TCC

#include "query.h"
#include "dballe/core/stlutils.h"
#include <algorithm>
#ifdef TRACE_QUERY
#include <cstdio>
#endif

namespace dballe {
namespace memdb {

namespace match {

template<typename T>
And<T>::~And()
{
    for (typename std::vector<Match<T>*>::iterator i = matches.begin(); i != matches.end(); ++i)
        delete *i;
}

template<typename T>
bool And<T>::operator()(const T& val) const
{
    for (typename std::vector<Match<T>*>::const_iterator i = matches.begin(); i != matches.end(); ++i)
        if (!(**i)(val))
            return false;
    return true;
}

template<typename T>
bool Idx2Values<T>::operator()(const size_t& val) const
{
    const T* v = index[val];
    if (!v) return false;
    return next(*v);
}

template<typename T>
void FilterBuilder<T>::add(Match<T>* f)
{
    if (!filter)
        filter = f;
    else if (!is_and)
        filter = is_and = new And<T>(filter, f);
    else
        is_and->add(f);
}

}

template<typename T>
void Results<T>::add_union(std::auto_ptr< stl::Sequences<size_t> > seq)
{
    all = false;
    if (!others_to_intersect)
        others_to_intersect = new stl::Sequences<size_t>;
    others_to_intersect->add_union(seq);
}

template<typename T>
void Results<T>::add(size_t singleton)
{
    all = false;
    if (!others_to_intersect)
        others_to_intersect = new stl::Sequences<size_t>;
    others_to_intersect->add_singleton(singleton);
}

template<typename T>
void Results<T>::add(const std::set<size_t>& p)
{
    all = false;
    if (!indices)
        indices = new stl::SetIntersection<size_t>;
    indices->add(p);
}

template<typename T> template<typename K>
bool Results<T>::add(const Index<K>& index, const K& val)
{
    all = false;
    typename Index<K>::const_iterator i = index.find(val);
    if (i == index.end())
    {
        trace_query("Adding positions from index lookup: element not found\n");
        return false;
    }
    trace_query("Adding positions from index lookup: found %zu elements\n", i->second.size());
    this->add(i->second);
    return true;
}

template<typename T> template<typename K>
bool Results<T>::add(const Index<K>& index, const K& min, const K& max)
{
    all = false;
    auto_ptr< stl::Sequences<size_t> > sequences;
    for (typename Index<K>::const_iterator i = index.lower_bound(min);
            i != index.upper_bound(max); ++i)
    {
        trace_query("Adding positions from index lookup: found %zu elements\n", i->second.size());
        if (!sequences.get())
            sequences.reset(new stl::Sequences<size_t>);
        sequences->add(i->second);
    }
    if (sequences.get())
    {
        trace_query("Adding positions from index lookup: union of %zu sequences\n", sequences->size());
        add_union(sequences);
        return true;
    } else {
        trace_query("Adding positions from index lookup: no element not found\n");
        return false;
    }
}

template<typename T> template<typename OUTITER>
bool Results<T>::copy_valptrs_to(OUTITER res)
{
    if (all) return false;
    if (empty) return true;

    if (!others_to_intersect)
    {
        if (indices)
        {
            if (filter.get())
            {
                trace_query("Generating results with intersection and filter\n");
                const Match<T>& match(*filter);
                for (stl::SetIntersection<size_t>::const_iterator i = indices->begin();
                        i != indices->end(); ++i)
                {
                    const T* val = values[*i];
                    if (!match(*val)) continue;
                    *res = val;
                    ++res;
                }
            } else {
                trace_query("Generating results with intersection only\n");
                for (stl::SetIntersection<size_t>::const_iterator i = indices->begin();
                        i != indices->end(); ++i)
                {
                    const T* val = values[*i];
                    *res = val;
                    ++res;
                }
            }
            return true;
        } else {
            if (filter.get())
            {
                trace_query("Generating results with filter only\n");
                const Match<T>& match(*filter);
                for (typename ValueStorage<T>::index_iterator i = values.index_begin(); i != values.index_end(); ++i)
                {
                    const T* val = values[*i];
                    if (!match(*val)) continue;
                    *res = val;
                    ++res;
                }
                return true;
            } else {
                // Nothing to do: we don't filter on any constraint, so we just leave res as it is
                trace_query("Generating results leaving results as it is\n");
                return false;
            }
        }
    } else {
        if (indices)
        {
            trace_query("Generating results: adding filters to extra intersections\n");
            others_to_intersect->add(indices->begin(), indices->end());
        }
        auto_ptr< stl::Sequences<size_t> > sequences(others_to_intersect);
        others_to_intersect = 0;
        stl::Intersection<size_t> intersection;
        if (filter.get())
        {
            trace_query("Generating results with extra intersections and filter\n");
            const Match<T>& match(*filter);
            for (stl::Intersection<size_t>::const_iterator i = intersection.begin(sequences); i != intersection.end(); ++i)
            {
                const T* val = values[*i];
                if (!match(*val)) continue;
                *res = val;
                ++res;
            }
        } else {
            trace_query("Generating results with extra intersections\n");
            for (stl::Intersection<size_t>::const_iterator i = intersection.begin(sequences); i != intersection.end(); ++i)
            {
                const T* val = values[*i];
                *res = val;
                ++res;
            }
        }
        return true;
    }
}

}
}

#include "dballe/core/stlutils.tcc"
#endif
