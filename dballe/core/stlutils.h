/*
 * core/stlutils - Useful functions to work with the STL
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

#ifndef DBA_CORE_STLUTILS_H
#define DBA_CORE_STLUTILS_H

#include <vector>

namespace dballe {
namespace stl {

/**
 * Virtual container containing the intersection of an arbitrary number of
 * sorted (begin, end) sequences.
 */
template<class ITER>
class Intersection
{
protected:
    struct Sequence
    {
        ITER begin;
        ITER end;

        Sequence(const ITER& begin, const ITER& end)
            : begin(begin), end(end) {}

        bool valid() const { return begin != end; }
        const typename ITER::value_type& operator*() const { return *begin; }
        void next() { ++begin; }
        bool operator==(const Sequence& s) const
        {
            return begin == s.begin && end == s.end;
        }
        bool operator!=(const Sequence& s) const
        {
            return begin != s.begin || end != s.end;
        }
    };

    std::vector<Sequence> sequences;

public:
    struct const_iterator
    {
        std::vector<Sequence> iters;

        // Begin iterator
        const_iterator(const std::vector<Sequence>& seqs) : iters(seqs)
        {
            sync_iters();
        }
        // End iterator
        const_iterator() {}

        const typename ITER::value_type& operator*() const
        {
            return *iters[0].begin;
        }

        const_iterator& operator++()
        {
            iters[0].next();
            sync_iters();
            return *this;
        }

        bool operator==(const const_iterator& i) const
        {
            return iters == i.iters;
        }

        bool operator!=(const const_iterator& i) const
        {
            return iters != i.iters;
        }

        // Advance iterators so that they all point to items of the same value,
        // or so that we become the end iterator
        void sync_iters()
        {
            while (true)
            {
                typename std::vector<Sequence>::iterator i = iters.begin();
                const typename ITER::value_type& candidate = **i;

                for ( ; i != iters.end(); ++i)
                {
                    while (i->valid() && **i < candidate)
                        i->next();
                    // When we reach the end of a sequence, we are done
                    if (!i->valid())
                    {
                        iters.clear();
                        return;
                    }
                    if (**i > candidate)
                    {
                        iters[0].next();
                        if (!iters[0].valid())
                        {
                            iters.clear();
                            return;
                        }
                        // Continue at the while level
                        goto next_round;
                    }
                }

                // All sequences have the same item: stop.
                return;
                next_round: ;
            }
        }
    };

    const_iterator begin() const
    {
        return const_iterator(sequences);
    }

    const_iterator end() const
    {
        return const_iterator();
    }

    void add(const ITER& begin, const ITER& end)
    {
        sequences.push_back(Sequence(begin, end));
    }
};

}
}

#endif
