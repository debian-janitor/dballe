/*
 * msg/defs - Common definitions
 *
 * Copyright (C) 2010--2014  ARPA-SIM <urpsim@smr.arpa.emr.it>
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

#ifndef DBA_MSG_DEFS_H
#define DBA_MSG_DEFS_H

/** @file
 * @ingroup msg
 *
 * Common definitions
 */

#include <limits.h>
#include <string>
#include <iosfwd>

namespace dballe {

/**
 * Supported encodings
 */
typedef enum {
	BUFR = 0,
	CREX = 1,
	AOF = 2,
} Encoding;

const char* encoding_name(Encoding enc);
Encoding parse_encoding(const std::string& s);

/**
 * Value to use for missing levels and time range components
 */
static const int MISSING_INT = INT_MAX;

struct Level
{
    /** Type of the first level.  See @ref level_table. */
    int ltype1;
    /** L1 value of the level.  See @ref level_table. */
    int l1;
    /** Type of the second level.  See @ref level_table. */
    int ltype2;
    /** L2 value of the level.  See @ref level_table. */
    int l2;

    Level(int ltype1=MISSING_INT, int l1=MISSING_INT, int ltype2=MISSING_INT, int l2=MISSING_INT)
        : ltype1(ltype1), l1(l1), ltype2(ltype2), l2(l2) {}
    Level(const char* ltype1, const char* l1=NULL, const char* ltype2=NULL, const char* l2=NULL);

    bool operator==(const Level& l) const
    {
        return ltype1 == l.ltype1 && l1 == l.l1
            && ltype2 == l.ltype2 && l2 == l.l2;
    }

    bool operator!=(const Level& l) const
    {
        return ltype1 != l.ltype1 || l1 != l.l1
            || ltype2 != l.ltype2 || l2 != l.l2;
    }

    bool operator<(const Level& l) const { return compare(l) < 0; }
    bool operator>(const Level& l) const { return compare(l) > 0; }

    /**
     * Compare two Level strutures, for use in sorting.
     *
     * @return
     *   -1 if *this < l, 0 if *this == l, 1 if *this > l
     */
    int compare(const Level& l) const
    {
        int res;
        if ((res = ltype1 - l.ltype1)) return res;
        if ((res = l1 - l.l1)) return res;
        if ((res = ltype2 - l.ltype2)) return res;
        return l2 - l.l2;
    }

    /**
     * Return a string description of this level
     */
    std::string describe() const;

    void format(std::ostream& out, const char* undef="-") const;

    static inline Level cloud(int ltype2=MISSING_INT, int l2=MISSING_INT) { return Level(256, MISSING_INT, ltype2, l2); }
    static inline Level waves(int ltype2=MISSING_INT, int l2=MISSING_INT) { return Level(264, MISSING_INT, ltype2, l2); }
    static inline Level ana() { return Level(); }
};

std::ostream& operator<<(std::ostream& out, const Level& l);

struct Trange
{
    /** Time range type indicator.  See @ref trange_table. */
    int pind;
    /** Time range P1 indicator.  See @ref trange_table. */
    int p1;
    /** Time range P2 indicator.  See @ref trange_table. */
    int p2;

    Trange(int pind=MISSING_INT, int p1=MISSING_INT, int p2=MISSING_INT)
        : pind(pind), p1(p1), p2(p2) {}
    Trange(const char* pind, const char* p1=NULL, const char* p2=NULL);

    bool operator==(const Trange& tr) const
    {
        return pind == tr.pind && p1 == tr.p1 && p2 == tr.p2;
    }

    bool operator!=(const Trange& tr) const
    {
        return pind != tr.pind || p1 != tr.p1 || p2 != tr.p2;
    }

    bool operator<(const Trange& t) const { return compare(t) < 0; }
    bool operator>(const Trange& t) const { return compare(t) > 0; }

    /**
     * Compare two Trange strutures, for use in sorting.
     *
     * @return
     *   -1 if *this < t, 0 if *this == t, 1 if *this > t
     */
    int compare(const Trange& t) const
    {
        int res;
        if ((res = pind - t.pind)) return res;
        if ((res = p1 - t.p1)) return res;
        return p2 - t.p2;
    }

    /**
     * Return a string description of this time range
     */
    std::string describe() const;

    void format(std::ostream& out, const char* undef="-") const;

    static inline Trange instant() { return Trange(254, 0, 0); }
    static inline Trange ana() { return Trange(); }
};

std::ostream& operator<<(std::ostream& out, const Trange& l);

/// Coordinates
struct Coord
{
    /// Latitude multiplied by 100000 (5 significant digits preserved)
    int lat;
    /// Longitude normalised from -180.0 to 180.0 and multiplied by 100000 (5
    /// significant digits preserved) 
    int lon;

    Coord() {}
    Coord(int lat, int lon);
    Coord(double lat, double lon);

    double dlat() const;
    double dlon() const;

    bool operator<(const Coord& o) const { return compare(o) < 0; }
    bool operator>(const Coord& o) const { return compare(o) > 0; }

    bool operator==(const Coord& c) const
    {
        return lat == c.lat && lon == c.lon;
    }

    bool operator!=(const Coord& c) const
    {
        return lat != c.lat || lon != c.lon;
    }

    /**
     * Compare two Coords strutures, for use in sorting.
     *
     * @return
     *   -1 if *this < o, 0 if *this == o, 1 if *this > o
     */
    int compare(const Coord& o) const
    {
        if (int res = lat - o.lat) return res;
        return lon - o.lon;
    }

    // Normalise longitude values to the [-180..180[ interval
    static int normalon(int lon);
    static double fnormalon(double lon);
};

std::ostream& operator<<(std::ostream& out, const Coord& c);

// Simple date structure
struct Date
{
    unsigned short year;
    unsigned char month;
    unsigned char day;

    Date() : year(0xffff), month(0xff), day(0xff) {}
    Date(unsigned short year, unsigned char month=1, unsigned char day=1)
        : year(year), month(month), day(day)
    {
    }
    Date(const Date& d) : year(d.year), month(d.month), day(d.day) {}
    Date(const int* val)
        : year(val[0]), month(val[1]), day(val[2])
    {
    }

    void fill(int* vals) const
    {
        vals[0] = (unsigned)year;
        vals[1] = (unsigned)month;
        vals[2] = (unsigned)day;
    }

    int compare(const Date& o) const
    {
        if (int res = year - o.year) return res;
        if (int res = month - o.month) return res;
        return day - o.day;
    }

    bool operator<(const Date& dt) const;
    bool operator>(const Date& dt) const;
    bool operator==(const Date& dt) const;
    bool operator!=(const Date& dt) const;

    static int days_in_month(int year, int month);
};

std::ostream& operator<<(std::ostream& out, const Date& dt);

struct Time
{
    unsigned char hour;
    unsigned char minute;
    unsigned char second;

    Time() : hour(0xff), minute(0xff), second(0xff) {}
    Time(unsigned char hour, unsigned char minute=0, unsigned char second=0)
        : hour(hour), minute(minute), second(second) {}
    Time(const Time& d) : hour(d.hour), minute(d.minute), second(d.second) {}
    Time(const int* val)
        : hour(val[0]), minute(val[1]), second(val[2])
    {
    }

    void fill(int* vals) const
    {
        vals[0] = (unsigned)hour;
        vals[1] = (unsigned)minute;
        vals[2] = (unsigned)second;
    }

    int compare(const Time& o) const
    {
        if (int res = hour - o.hour) return res;
        if (int res = minute - o.minute) return res;
        return second - o.second;
    }

    bool operator<(const Time& dt) const;
    bool operator>(const Time& dt) const;
    bool operator==(const Time& dt) const;
    bool operator!=(const Time& dt) const;
};

std::ostream& operator<<(std::ostream& out, const Time& t);

/// Simple datetime structure
struct Datetime
{
    Date date;
    Time time;

    Datetime() {}
    Datetime(const Date& date, const Time& time) : date(date), time(time) {}
    Datetime(unsigned short year, unsigned char month=1, unsigned char day=1,
             unsigned char hour=0, unsigned char minute=0, unsigned char second=0)
        : date(year, month, day), time(hour, minute, second) {}
    Datetime(const int* val) : date(val), time(val+3) {}

    int compare(const Datetime& o) const
    {
        if (int res = date.compare(o.date)) return res;
        return time.compare(o.time);
    }

    void fill(int* vals) const
    {
        date.fill(vals);
        time.fill(vals + 3);
    }

    bool operator==(const Datetime& dt) const;
    bool operator!=(const Datetime& dt) const;
    bool operator<(const Datetime& dt) const;
    bool operator>(const Datetime& dt) const;
};

std::ostream& operator<<(std::ostream& out, const Datetime& dt);

}

// vim:set ts=4 sw=4:
#endif
