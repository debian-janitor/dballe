/*
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

#include "dballe/msg/wr_codec.h"
#include "dballe/msg/msg.h"
#include "dballe/msg/context.h"
#include <wreport/bulletin.h>
#include <wreport/vartable.h>
#include <wreport/conv.h>
#include <wreport/codetables.h>
#include <wreport/notes.h>

using namespace wreport;
using namespace std;

#define TEMP_WMO_NAME "temp-wmo"
#define TEMP_WMO_DESC "Temp WMO (2.101)"

#define TEMP_ECMWF_LAND_NAME "temp-ecmwf-land"
#define TEMP_ECMWF_LAND_DESC "Temp ECMWF land (2.101)"

#define TEMP_ECMWF_SHIP_NAME "temp-ecmwf-ship"
#define TEMP_ECMWF_SHIP_DESC "Temp ECMWF ship (2.102)"

#define TEMP_RADAR_NAME "temp-radar"
#define TEMP_RADAR_DESC "Temp radar doppler wind profile (6.1)"

#define PILOT_WMO_NAME "pilot-wmo"
#define PILOT_WMO_DESC "Pilot (2.1, 2.2, 2.3)"

#define PILOT_ECMWF_NAME "pilot-ecmwf"
#define PILOT_ECMWF_DESC "Pilot (2.91)"


namespace dballe {
namespace msg {
namespace wr {

namespace {

// Base template for vertical soundings
struct TempBase : public Template
{
    bool is_crex;

    TempBase(const ExporterOptions& opts, const Messages& msgs)
        : Template(opts, msgs) {}

    /// Count the number of sounding levels
    int count_levels() const
    {
        int lev_no = 0;
        for (size_t i = 0; i < msg->data.size(); ++i)
        {
            if (msg->data[i]->find_vsig() != NULL)
                ++lev_no;
        }
        return lev_no;
    }

    int add_sounding_levels()
    {
        int count = count_levels();
        subset->store_variable_i(WR_VAR(0, 31,  1), count);

        // Iterate backwards as we need to add levels in decreasing pressure order
        for (int i = msg->data.size() - 1; i >= 0; --i)
        {
            msg::Context& c = *msg->data[i];

            // We only want levels with a vertical sounding significance
            const Var* vss = c.find_vsig();
            if (vss == NULL) continue;

            /* We only want pressure levels */
            if (c.level.ltype1 != 100) continue;
            double press = c.level.l1;

            /* Add pressure */
            const Var* press_var = c.find(WR_VAR(0, 10, 4));
            if (!press_var)
                press_var = c.find(WR_VAR(0, 7, 4));
            if (press_var)
                subset->store_variable(WR_VAR(0, 7, 4), *press_var);
            else if (press == MISSING_INT)
                subset->store_variable_undef(WR_VAR(0, 7, 4));
            else
                subset->store_variable_d(WR_VAR(0, 7, 4), press);

            /* Add vertical sounding significance */
            {
                int vssval = convert_BUFR08042_to_BUFR08001(vss->enqi());
                if (vssval & BUFR08042::MISSING)
                {
                    // Deal with 'missing VSS' group markers
                    subset->store_variable_undef(WR_VAR(0, 8, 1));
                } else {
                    Var nvar(subset->tables->btable->query(WR_VAR(0, 8, 1)), vssval);
                    nvar.setattrs(*vss);
                    subset->store_variable(WR_VAR(0, 8, 1), nvar);
                }
            }

            /* Add the rest */
            add(WR_VAR(0, 10, 3), &c, WR_VAR(0, 10,   8));
            add(WR_VAR(0, 12, 1), &c, WR_VAR(0, 12, 101));
            add(WR_VAR(0, 12, 3), &c, WR_VAR(0, 12, 103));
            add(WR_VAR(0, 11, 1), &c, WR_VAR(0, 11,   1));
            add(WR_VAR(0, 11, 2), &c, WR_VAR(0, 11,   2));
        }

        return count;
    }

    virtual void setupBulletin(wreport::Bulletin& bulletin)
    {
        Template::setupBulletin(bulletin);

        // Use old table for old templates
        if (BufrBulletin* b = dynamic_cast<BufrBulletin*>(&bulletin))
        {
            b->master_table_version_number = 13;
        }

        is_crex = dynamic_cast<CrexBulletin*>(&bulletin) != 0;

        bulletin.data_category = 2;
        bulletin.data_subcategory = 255;

        // Scan to see what we are dealing with
        bulletin.data_subcategory_local = 101;
        for (const auto& mi: msgs)
        {
            auto msg = Msg::downcast(mi);
            if (msg->type == MessageType::PILOT)
            {
                bulletin.data_subcategory_local = 91;
                break;
            } else if (msg->get_ident_var()) {
                bulletin.data_subcategory_local = 102;
                break;
            } else if (msg->get_block_var()) {
                break;
            }
        }
    }

    virtual void to_subset(const Msg& msg, wreport::Subset& subset)
    {
        Template::to_subset(msg, subset);
    }

    bool do_D03051(const msg::Context& c)
    {
        add(WR_VAR(0,  4, 86), &c);
        add(WR_VAR(0,  8, 42), &c);
        if (c.level.l1 == MISSING_INT)
            subset->store_variable_undef(WR_VAR(0, 7, 4));
        else
            subset->store_variable_d(WR_VAR(0, 7, 4), c.level.l1);
        add(WR_VAR(0,  5, 15), &c);
        add(WR_VAR(0,  6, 15), &c);
        add(WR_VAR(0, 11, 61), &c);
        add(WR_VAR(0, 11, 62), &c);
        return true;
    }

    bool do_D03053(const msg::Context& c)
    {
        add(WR_VAR(0,  4, 86), &c);
        add(WR_VAR(0,  8, 42), &c);
        if (c.level.l1 == MISSING_INT)
            subset->store_variable_undef(WR_VAR(0, 7, 9));
        else
            subset->store_variable_d(WR_VAR(0, 7, 9), c.level.l1);
        add(WR_VAR(0,  5, 15), &c);
        add(WR_VAR(0,  6, 15), &c);
        add(WR_VAR(0, 11, 61), &c);
        add(WR_VAR(0, 11, 62), &c);
        return true;
    }
};

struct TempWMO : public TempBase
{
    TempWMO(const ExporterOptions& opts, const Messages& msgs)
        : TempBase(opts, msgs) {}

    virtual const char* name() const { return TEMP_WMO_NAME; }
    virtual const char* description() const { return TEMP_WMO_DESC; }

    virtual void setupBulletin(wreport::Bulletin& bulletin)
    {
        TempBase::setupBulletin(bulletin);

        bulletin.data_category = 2;
        bulletin.data_subcategory = 4;
        bulletin.data_subcategory_local = 255;
        for (const auto& mi : msgs)
        {
            auto msg = Msg::downcast(mi);
            if (msg->type == MessageType::TEMP_SHIP)
            {
                bulletin.data_subcategory = 5;
                break;
            }
        }

        // Data descriptor section
        bulletin.datadesc.clear();
        bulletin.datadesc.push_back(WR_VAR(3, 9, 52));
        bulletin.load_tables();
    }

    void do_D03054(const msg::Context& c)
    {
        add(WR_VAR(0,  4,  86), &c);
        if (const Var* vss = c.find(WR_VAR(0, 8, 42)))
        {
            // Deal with 'missing VSS' group markers
            if (vss->enq((int)BUFR08042::MISSING) & BUFR08042::MISSING)
                subset->store_variable_undef(WR_VAR(0, 8, 42));
            else
                subset->store_variable(*vss);
        }
        else
            subset->store_variable_undef(WR_VAR(0, 8, 42));
        switch (c.level.ltype1)
        {
            case 100:
                if (c.level.l1 == MISSING_INT)
                    subset->store_variable_undef(WR_VAR(0, 7, 4));
                else
                    subset->store_variable_d(WR_VAR(0, 7, 4), c.level.l1);
                add(WR_VAR(0, 10, 9), &c, WR_VAR(0, 10, 8));
                break;
            case 102:
                add(WR_VAR(0, 7, 4), &c, WR_VAR(0, 10, 4));
                if (const Var* var = c.find(WR_VAR(0, 10, 8)))
                    add(WR_VAR(0, 10, 9), var);
                else
                    subset->store_variable_d(WR_VAR(0, 10, 9), (double)c.level.l1);
                break;
            case 103:
                add(WR_VAR(0, 7, 4), &c, WR_VAR(0, 10, 4));
                add(WR_VAR(0, 10, 9), &c, WR_VAR(0, 10, 8));
                break;
        }
        add(WR_VAR(0,  5,  15), &c);
        add(WR_VAR(0,  6,  15), &c);
        add(WR_VAR(0, 12, 101), &c);
        add(WR_VAR(0, 12, 103), &c);
        add(WR_VAR(0, 11,   1), &c);
        add(WR_VAR(0, 11,   2), &c);
    }

    virtual void to_subset(const Msg& msg, wreport::Subset& subset)
    {
        TempBase::to_subset(msg, subset);
        do_D01001(); // station id
        add(WR_VAR(0,  1, 11), c_station, DBA_MSG_IDENT);
        add(WR_VAR(0,  2, 11), c_gnd_instant, DBA_MSG_SONDE_TYPE);
        add(WR_VAR(0,  2, 13), c_gnd_instant, DBA_MSG_SONDE_CORRECTION);
        add(WR_VAR(0,  2, 14), c_gnd_instant, DBA_MSG_SONDE_TRACKING);
        add(WR_VAR(0,  2,  3), c_gnd_instant, DBA_MSG_MEAS_EQUIP_TYPE);
        subset.store_variable_i(WR_VAR(0, 8, 21), 18);
        do_D01011(); // date
        do_D01013(); // time
        do_D01021(); // coordinates
        add(WR_VAR(0,  7, 30), c_station, DBA_MSG_HEIGHT_STATION);
        add(WR_VAR(0,  7, 31), c_station, DBA_MSG_HEIGHT_BARO);
        add(WR_VAR(0,  7,  7), c_station, DBA_MSG_HEIGHT_RELEASE);
        add(WR_VAR(0, 33, 24), c_station, DBA_MSG_STATION_HEIGHT_QUALITY);

        // Cloud information reported with vertical soundings
        add(WR_VAR(0,  8,  2), WR_VAR(0, 8, 2), Level::cloud(258, 0), Trange::instant());
        add(WR_VAR(0, 20, 11), DBA_MSG_CLOUD_NH);
        add(WR_VAR(0, 20, 13), DBA_MSG_CLOUD_HH);
        add(WR_VAR(0, 20, 12), DBA_MSG_CLOUD_CL);
        add(WR_VAR(0, 20, 12), DBA_MSG_CLOUD_CM);
        add(WR_VAR(0, 20, 12), DBA_MSG_CLOUD_CH);
        subset.store_variable_undef(WR_VAR(0, 8, 2));
        add(WR_VAR(0, 22, 43), c_gnd_instant, DBA_MSG_WATER_TEMP);

        // Undef for now, we fill it later
        size_t rep_count_pos = subset.size();
        subset.store_variable_undef(WR_VAR(0, 31, 2));

        // Temperature, dew-point and wind data at pressure levels
        int group_count = 0;
        for (std::vector<msg::Context*>::const_reverse_iterator i = msg.data.rbegin();
                i != msg.data.rend(); ++i)
        {
            // Iterate backwards to get pressure levels sorted from the higher
            // to the lower pressure
            const msg::Context* c = *i;
            // Skip non-presure levels
            if (!c->find_vsig()) continue;
            // Skip levels not for us
            if (!(
                  c->find(WR_VAR(0, 10,   8))
               || c->find(WR_VAR(0, 12, 101))
               || c->find(WR_VAR(0, 12, 103))
               || c->find(WR_VAR(0, 11,   1))
               || c->find(WR_VAR(0, 11,   2))) && (
                  c->find(WR_VAR(0, 11, 61))
               || c->find(WR_VAR(0, 11, 62)))
               )
                continue;
            do_D03054(*c);
            ++group_count;
        }
        subset[rep_count_pos].seti(group_count);

        // Wind shear data
        rep_count_pos = subset.size();
        subset.store_variable_undef(WR_VAR(0, 31, 1));
        group_count = 0;
        for (std::vector<msg::Context*>::const_reverse_iterator i = msg.data.rbegin();
                i != msg.data.rend(); ++i)
        {
            // Iterate backwards to get pressure levels sorted from the higher
            // to the lower pressure
            const msg::Context* c = *i;
            // Skip non-pressure levels
            if (c->level.ltype1 != 100) continue;
            // Skip levels not for us
            if (!(
                  c->find(WR_VAR(0, 11, 61))
               || c->find(WR_VAR(0, 11, 62))))
                continue;
            if (do_D03051(*c))
                ++group_count;
        }
        subset[rep_count_pos].seti(group_count);
    }
};

struct TempRadar : public TempBase
{
    TempRadar(const ExporterOptions& opts, const Messages& msgs)
        : TempBase(opts, msgs) {}

    virtual const char* name() const { return TEMP_RADAR_NAME; }
    virtual const char* description() const { return TEMP_RADAR_DESC; }

    virtual void setupBulletin(wreport::Bulletin& bulletin)
    {
        TempBase::setupBulletin(bulletin);

        bulletin.data_category = 6;
        bulletin.data_subcategory = 1;
        bulletin.data_subcategory_local = 255;

        // Data descriptor section
        bulletin.datadesc.clear();
        bulletin.datadesc.push_back(WR_VAR(0,  1,  33));
        bulletin.datadesc.push_back(WR_VAR(0,  1,  34));
        bulletin.datadesc.push_back(WR_VAR(3,  1,   1));
        bulletin.datadesc.push_back(WR_VAR(3,  1,  11));
        bulletin.datadesc.push_back(WR_VAR(3,  1,  12));
        bulletin.datadesc.push_back(WR_VAR(3,  1,  22));
        bulletin.datadesc.push_back(WR_VAR(0,  2,   3));
        bulletin.datadesc.push_back(WR_VAR(0,  2, 121));
        bulletin.datadesc.push_back(WR_VAR(0,  2, 125));
        bulletin.datadesc.push_back(WR_VAR(1,  7,   0));
        bulletin.datadesc.push_back(WR_VAR(0, 31,   1));
        bulletin.datadesc.push_back(WR_VAR(0,  7,   7));
        bulletin.datadesc.push_back(WR_VAR(0, 11,   1));
        bulletin.datadesc.push_back(WR_VAR(0, 11,   2));
        bulletin.datadesc.push_back(WR_VAR(0, 33,   2));
        bulletin.datadesc.push_back(WR_VAR(0, 11,   6));
        bulletin.datadesc.push_back(WR_VAR(0, 33,   2));
        bulletin.datadesc.push_back(WR_VAR(0, 11,  50));
        bulletin.load_tables();
    }

    virtual void to_subset(const Msg& msg, wreport::Subset& subset)
    {
        TempBase::to_subset(msg, subset);
        add(WR_VAR(0, 1, 33), c_station);
        add(WR_VAR(0, 1, 34), c_station);
        do_D01001(); // station id
        do_D01011(); // date
        do_D01012(); // date
        do_D01022(); // date
        add(WR_VAR(0,  2,  3), c_gnd_instant, DBA_MSG_MEAS_EQUIP_TYPE);
        add(WR_VAR(0, 2, 121), c_gnd_instant);
        add(WR_VAR(0, 2, 125), c_gnd_instant);

        // Undef for now, we fill it later
        size_t rep_count_pos = subset.size();
        subset.store_variable_undef(WR_VAR(0, 31, 1));

        // Sounding levels
        int group_count = 0;
        for (std::vector<msg::Context*>::const_reverse_iterator i = msg.data.rbegin();
                i != msg.data.rend(); ++i)
        {
            // Iterate backwards to get pressure levels sorted from the higher
            // to the lower pressure
            const msg::Context* c = *i;
            // We are only interested in height levels
            if (c->level.ltype1 != 102) continue;
            if (const Var* var = c->find(WR_VAR(0, 7, 7)))
                add(WR_VAR(0, 7, 7), var);
            else
                subset.store_variable_d(WR_VAR(0, 7, 7), c->level.l1/1000.0);
            add(WR_VAR(0, 11,   1), c);
            static const Varcode codes[] = { WR_VAR(0, 11,   2), WR_VAR(0, 11,   6) };
            for (unsigned i = 0; i < 2; ++i)
            {
                Varcode code = codes[i];
                if (const wreport::Var *var = c->find(code))
                {
                    add(code, var);
                    add(WR_VAR(0, 33, 2), var->enqa(WR_VAR(0, 33,  2)));
                } else {
                    subset.store_variable_undef(code);
                    subset.store_variable_undef(WR_VAR(0, 33,   2));
                }
            }
            add(WR_VAR(0, 11,  50), c);
            ++group_count;
        }
        subset[rep_count_pos].seti(group_count);
    }
};

struct TempEcmwfLand : public TempBase
{
    TempEcmwfLand(const ExporterOptions& opts, const Messages& msgs)
        : TempBase(opts, msgs) {}

    virtual const char* name() const { return TEMP_ECMWF_LAND_NAME; }
    virtual const char* description() const { return TEMP_ECMWF_LAND_DESC; }

    virtual void setupBulletin(wreport::Bulletin& bulletin)
    {
        TempBase::setupBulletin(bulletin);

        // Data descriptor section
        bulletin.datadesc.clear();
        bulletin.datadesc.push_back(WR_VAR(3,  9,  7));
        if (!is_crex)
        {
                bulletin.datadesc.push_back(WR_VAR(2, 22,   0));
                bulletin.datadesc.push_back(WR_VAR(1,  1,   0));
                bulletin.datadesc.push_back(WR_VAR(0, 31,   2));
                bulletin.datadesc.push_back(WR_VAR(0, 31,  31));
                bulletin.datadesc.push_back(WR_VAR(0,  1,  31));
                bulletin.datadesc.push_back(WR_VAR(0,  1,  32));
                bulletin.datadesc.push_back(WR_VAR(1,  1,   0));
                bulletin.datadesc.push_back(WR_VAR(0, 31,   2));
                bulletin.datadesc.push_back(WR_VAR(0, 33,   7));
        }

        bulletin.load_tables();
    }

    virtual void to_subset(const Msg& msg, wreport::Subset& subset)
    {
        TempBase::to_subset(msg, subset);
        /*  0 */ add(WR_VAR(0,  1,  1), DBA_MSG_BLOCK);
        /*  1 */ add(WR_VAR(0,  1,  2), DBA_MSG_STATION);
        /*  2 */ add(WR_VAR(0,  2, 11), DBA_MSG_SONDE_TYPE);
        /*  3 */ add(WR_VAR(0,  2, 12), DBA_MSG_SONDE_METHOD);
        do_D01011();
        do_D01012();
        /*  9 */ add(WR_VAR(0,  5,  1), DBA_MSG_LATITUDE);
        /* 10 */ add(WR_VAR(0,  6,  1), DBA_MSG_LONGITUDE);
        /* 11 */ add(WR_VAR(0,  7,  1), DBA_MSG_HEIGHT_STATION);
        /* 12 */ add(WR_VAR(0, 20, 10), DBA_MSG_CLOUD_N);
        /* 13 */ add(WR_VAR(0,  8,  2), WR_VAR(0, 8, 2), Level::cloud(258, 0), Trange::instant());
        /* 14 */ add(WR_VAR(0, 20, 11), DBA_MSG_CLOUD_NH);
        /* 15 */ add(WR_VAR(0, 20, 13), DBA_MSG_CLOUD_HH);
        /* 16 */ add(WR_VAR(0, 20, 12), DBA_MSG_CLOUD_CL);
        /* 17 */ add(WR_VAR(0, 20, 12), DBA_MSG_CLOUD_CM);
        /* 18 */ add(WR_VAR(0, 20, 12), DBA_MSG_CLOUD_CH);
        add_sounding_levels();
        if (!is_crex)
        {
            int count = subset.append_dpb(WR_VAR(2, 22, 0), subset.size(), WR_VAR(0, 33, 7));
            if (opts.centre != MISSING_INT)
                subset.store_variable_i(WR_VAR(0, 1, 31), opts.centre);
            else
                subset.store_variable_undef(WR_VAR(0, 1, 31));
            if (opts.application != MISSING_INT)
                subset.store_variable_i(WR_VAR(0, 1, 32), opts.application);
            else
                subset.store_variable_undef(WR_VAR(0, 1, 32));
            subset.store_variable_i(WR_VAR(0, 31, 2), count);
        }
    }
};

struct TempEcmwfShip : public TempBase
{
    TempEcmwfShip(const ExporterOptions& opts, const Messages& msgs)
        : TempBase(opts, msgs) {}

    virtual const char* name() const { return TEMP_ECMWF_SHIP_NAME; }
    virtual const char* description() const { return TEMP_ECMWF_SHIP_DESC; }

    virtual void setupBulletin(wreport::Bulletin& bulletin)
    {
        TempBase::setupBulletin(bulletin);

        // Data descriptor section
        bulletin.datadesc.clear();

        /* FIXME: some expansions are not in CREX D tables yet, so we need to use
         * the expanded version here.  The code needs to be changed when newer CREX
         * D tables are available */
        /* WR_VAR(3,  9, 196), Replaced with expansion: */
                bulletin.datadesc.push_back(WR_VAR(3, 1,  3));
                bulletin.datadesc.push_back(WR_VAR(0, 2, 11));
                bulletin.datadesc.push_back(WR_VAR(0, 2, 12));
                bulletin.datadesc.push_back(WR_VAR(3, 1, 11));
                bulletin.datadesc.push_back(WR_VAR(3, 1, 12));
                bulletin.datadesc.push_back(WR_VAR(3, 1, 23));
                bulletin.datadesc.push_back(WR_VAR(0, 7,  1));
        bulletin.datadesc.push_back(WR_VAR(3,  2,  4));
        bulletin.datadesc.push_back(WR_VAR(1,  1,  0));
        bulletin.datadesc.push_back(WR_VAR(0, 31,  1));
        bulletin.datadesc.push_back(WR_VAR(3,  3, 14));
        if (!is_crex)
        {
                bulletin.datadesc.push_back(WR_VAR(2, 22,  0));
                bulletin.datadesc.push_back(WR_VAR(1,  1,  0));
                bulletin.datadesc.push_back(WR_VAR(0, 31,  2));
                bulletin.datadesc.push_back(WR_VAR(0, 31, 31));
                bulletin.datadesc.push_back(WR_VAR(0,  1, 31));
                bulletin.datadesc.push_back(WR_VAR(0,  1, 32));
                bulletin.datadesc.push_back(WR_VAR(1,  1,  0));
                bulletin.datadesc.push_back(WR_VAR(0, 31,  2));
                bulletin.datadesc.push_back(WR_VAR(0, 33,  7));
        }

        bulletin.load_tables();
    }

    virtual void to_subset(const Msg& msg, wreport::Subset& subset)
    {
        TempBase::to_subset(msg, subset);
        /*  0 */ add(WR_VAR(0,  1, 11), DBA_MSG_IDENT);
        /*  1 */ add(WR_VAR(0,  1, 12), DBA_MSG_ST_DIR);
        /*  2 */ add(WR_VAR(0,  1, 13), DBA_MSG_ST_SPEED);
        /*  3 */ add(WR_VAR(0,  2, 11), DBA_MSG_SONDE_TYPE);
        /*  4 */ add(WR_VAR(0,  2, 12), DBA_MSG_SONDE_METHOD);
        do_D01011();
        do_D01012();
        /* 10 */ add(WR_VAR(0,  5,  2), DBA_MSG_LATITUDE);
        /* 11 */ add(WR_VAR(0,  6,  2), DBA_MSG_LONGITUDE);
        /* 12 */ add(WR_VAR(0,  7,  1), DBA_MSG_HEIGHT_STATION);
        /* 13 */ add(WR_VAR(0, 20, 10), DBA_MSG_CLOUD_N);
        /* 14 */ add(WR_VAR(0,  8,  2), WR_VAR(0, 8, 2), Level::cloud(258, 0), Trange::instant());
        /* 15 */ add(WR_VAR(0, 20, 11), DBA_MSG_CLOUD_NH);
        /* 16 */ add(WR_VAR(0, 20, 13), DBA_MSG_CLOUD_HH);
        /* 17 */ add(WR_VAR(0, 20, 12), DBA_MSG_CLOUD_CL);
        /* 18 */ add(WR_VAR(0, 20, 12), DBA_MSG_CLOUD_CM);
        /* 19 */ add(WR_VAR(0, 20, 12), DBA_MSG_CLOUD_CH);
        add_sounding_levels();
        if (!is_crex)
        {
            int count = subset.append_dpb(WR_VAR(2, 22, 0), subset.size(), WR_VAR(0, 33, 7));
            if (opts.centre != MISSING_INT)
                subset.store_variable_i(WR_VAR(0, 1, 31), opts.centre);
            else
                subset.store_variable_undef(WR_VAR(0, 1, 31));
            if (opts.application != MISSING_INT)
                subset.store_variable_i(WR_VAR(0, 1, 32), opts.application);
            else
                subset.store_variable_undef(WR_VAR(0, 1, 32));
            subset.store_variable_i(WR_VAR(0, 31, 2), count);
        }
    }
};

struct PilotWMO : public TempBase
{
    bool pressure_levs;

    PilotWMO(const ExporterOptions& opts, const Messages& msgs)
        : TempBase(opts, msgs) {}

    virtual const char* name() const { return PILOT_WMO_NAME; }
    virtual const char* description() const { return PILOT_WMO_DESC; }

    virtual void to_bulletin(wreport::Bulletin& bulletin)
    {
        // Scan contexts to see if we are encoding pressure or height levels
        unsigned has_press = 0;
        unsigned has_height = 0;

        for (const auto& mi: msgs)
        {
            auto msg = Msg::downcast(mi);
            for (const auto& ctx: msg->data)
            {
                switch (ctx->level.ltype1)
                {
                    case 100: ++has_press; break;
                    case 102: ++has_height; break;
                    case 103: ++has_height; break;
                }
            }
        }

        pressure_levs = has_press >= has_height;

        // Continue with normal operations
        TempBase::to_bulletin(bulletin);
    }

    virtual void setupBulletin(wreport::Bulletin& bulletin)
    {
        TempBase::setupBulletin(bulletin);

        bulletin.data_category = 2;
        bulletin.data_subcategory = 1;
        bulletin.data_subcategory_local = 255;

        // Data descriptor section
        bulletin.datadesc.clear();
        if (pressure_levs)
            bulletin.datadesc.push_back(WR_VAR(3, 9, 50)); // For pressure levels
        else
            bulletin.datadesc.push_back(WR_VAR(3, 9, 51)); // For height levels
        bulletin.load_tables();
    }
    virtual void to_subset(const Msg& msg, wreport::Subset& subset)
    {
        TempBase::to_subset(msg, subset);

        // Identification of launch site and instrumentation
        add(WR_VAR(0,  1,  1), DBA_MSG_BLOCK);
        add(WR_VAR(0,  1,  2), DBA_MSG_STATION);
        add(WR_VAR(0,  1, 11), DBA_MSG_IDENT);
        add(WR_VAR(0,  2, 11), DBA_MSG_SONDE_TYPE);
        add(WR_VAR(0,  2, 14), DBA_MSG_SONDE_TRACKING);
        add(WR_VAR(0,  2,  3), DBA_MSG_MEAS_EQUIP_TYPE);

        // Date/time of launch
        add(WR_VAR(0,  8, 21), 18); // Launch time
        do_D01011();
        do_D01013();

        // Horizontal and vertical coordinates of launch site
        add(WR_VAR(0,  5,  1), DBA_MSG_LATITUDE);
        add(WR_VAR(0,  6,  1), DBA_MSG_LONGITUDE);
        add(WR_VAR(0,  7, 30), DBA_MSG_HEIGHT_STATION);
        add(WR_VAR(0,  7, 31), DBA_MSG_HEIGHT_BARO);
        add(WR_VAR(0,  7,  7), DBA_MSG_HEIGHT_RELEASE);
        add(WR_VAR(0, 33, 24), DBA_MSG_STATION_HEIGHT_QUALITY);

        // Keep track of where we stored
        int rep_count_pos = subset.size();
        subset.store_variable_undef(WR_VAR(0, 31,  2));

        /* Iterate backwards as we need to add levels in decreasing pressure order */
        int group_count = 0;
        for (std::vector<msg::Context*>::const_reverse_iterator i = msg.data.rbegin();
                i != msg.data.rend(); ++i)
        {
            const msg::Context& c = **i;
            // We only want levels with a vertical sounding significance
            const Var* vss = c.find_vsig();
            if (vss == NULL) continue;
            if (pressure_levs && c.level.ltype1 != 100) continue;
            if (!pressure_levs && c.level.ltype1 != 102) continue;

            add(WR_VAR(0,  4, 86), &c);
            add(WR_VAR(0,  8, 42), &c);
            if (pressure_levs)
            {
                // Wind data at pressure levels
                if (c.level.l1 == MISSING_INT)
                    subset.store_variable_undef(WR_VAR(0, 7, 4));
                else
                    subset.store_variable_d(WR_VAR(0, 7, 4), c.level.l1);
            } else {
                // Wind data at heights
                if (c.level.l1 == MISSING_INT)
                    subset.store_variable_undef(WR_VAR(0, 7, 9));
                else
                    subset.store_variable_d(WR_VAR(0, 7, 9), c.level.l1);
            }
            add(WR_VAR(0,  5, 15), &c);
            add(WR_VAR(0,  6, 15), &c);
            add(WR_VAR(0, 11, 1), &c);
            add(WR_VAR(0, 11, 2), &c);
            ++group_count;
        }
        subset[rep_count_pos].seti(group_count);

        // Wind shear data
        rep_count_pos = subset.size();
        subset.store_variable_undef(WR_VAR(0, 31,  1));
        group_count = 0;
        for (std::vector<msg::Context*>::const_reverse_iterator i = msg.data.rbegin();
                i != msg.data.rend(); ++i)
        {
            // Iterate backwards to get pressure levels sorted from the higher
            // to the lower pressure
            const msg::Context& c = **i;
            // Skip levels that do not fit
            if (pressure_levs && c.level.ltype1 != 100) continue;
            if (!pressure_levs && c.level.ltype1 != 102) continue;
            if (pressure_levs)
            {
                if (do_D03051(c))
                    ++group_count;
            } else {
                if (do_D03053(c))
                    ++group_count;
            }
        }
        subset[rep_count_pos].seti(group_count);
    }
};

struct PilotEcmwf : public TempBase
{
    PilotEcmwf(const ExporterOptions& opts, const Messages& msgs)
        : TempBase(opts, msgs) {}

    virtual const char* name() const { return PILOT_ECMWF_NAME; }
    virtual const char* description() const { return PILOT_ECMWF_DESC; }

    virtual void setupBulletin(wreport::Bulletin& bulletin)
    {
        TempBase::setupBulletin(bulletin);

        // Data descriptor section
        bulletin.datadesc.clear();
        bulletin.datadesc.push_back(WR_VAR(3,  1,   1));
        bulletin.datadesc.push_back(WR_VAR(0,  2, 11));
        bulletin.datadesc.push_back(WR_VAR(0,  2, 12));
        bulletin.datadesc.push_back(WR_VAR(3,  1, 11));
        bulletin.datadesc.push_back(WR_VAR(3,  1, 12));
        bulletin.datadesc.push_back(WR_VAR(3,  1, 22));
        bulletin.datadesc.push_back(WR_VAR(1,  5,  0));
        bulletin.datadesc.push_back(WR_VAR(0, 31,  1));
        bulletin.datadesc.push_back(WR_VAR(0,  7,  4));
        bulletin.datadesc.push_back(WR_VAR(0,  8,  1));
        bulletin.datadesc.push_back(WR_VAR(0, 10,  3));
        bulletin.datadesc.push_back(WR_VAR(0, 11,  1));
        bulletin.datadesc.push_back(WR_VAR(0, 11,  2));
        if (!is_crex)
        {
                bulletin.datadesc.push_back(WR_VAR(2, 22,  0));
                bulletin.datadesc.push_back(WR_VAR(1,  1,  0));
                bulletin.datadesc.push_back(WR_VAR(0, 31,  2));
                bulletin.datadesc.push_back(WR_VAR(0, 31, 31));
                bulletin.datadesc.push_back(WR_VAR(0,  1, 31));
                bulletin.datadesc.push_back(WR_VAR(0,  1, 32));
                bulletin.datadesc.push_back(WR_VAR(1,  1,  0));
                bulletin.datadesc.push_back(WR_VAR(0, 31,  2));
                bulletin.datadesc.push_back(WR_VAR(0, 33,  7));
        }

        bulletin.load_tables();
    }
    virtual void to_subset(const Msg& msg, wreport::Subset& subset)
    {
        TempBase::to_subset(msg, subset);

        /*  0 */ add(WR_VAR(0,  1,  1), DBA_MSG_BLOCK);
        /*  1 */ add(WR_VAR(0,  1,  2), DBA_MSG_STATION);
        /*  2 */ add(WR_VAR(0,  2, 11), DBA_MSG_SONDE_TYPE);
        /*  3 */ add(WR_VAR(0,  2, 12), DBA_MSG_SONDE_METHOD);
        do_D01011();
        do_D01012();
        /*  9 */ add(WR_VAR(0,  5,  1), DBA_MSG_LATITUDE);
        /* 10 */ add(WR_VAR(0,  6,  1), DBA_MSG_LONGITUDE);
        /* 11 */ add(WR_VAR(0,  7,  1), DBA_MSG_HEIGHT_STATION);

        int rep_count_pos = subset.size();
        subset.store_variable_undef(WR_VAR(0, 31,  1));

        /* Iterate backwards as we need to add levels in decreasing pressure order */
        int group_count = 0;
        for (int i = msg.data.size() - 1; i >= 0; --i)
        {
            msg::Context& c = *msg.data[i];
            // We only want levels with a vertical sounding significance
            const Var* vss = c.find_vsig();
            if (vss == NULL) continue;

            /* Add pressure */
            if (const Var* var = c.find(WR_VAR(0, 10, 4)))
                subset.store_variable(WR_VAR(0,  7,  4), *var);
            else if (c.level.ltype1 == 100)
                subset.store_variable_d(WR_VAR(0,  7,  4), c.level.l1);
            else
                subset.store_variable_undef(WR_VAR(0, 7, 4));

            /* Add vertical sounding significance */
            {
                Var nvar(subset.tables->btable->query(WR_VAR(0, 8, 1)), (int)convert_BUFR08042_to_BUFR08001(vss->enqi()));
                nvar.setattrs(*vss);
                subset.store_variable(WR_VAR(0, 8, 1), nvar);
            }

            /* Add geopotential */
            if (const Var* var = c.find(WR_VAR(0, 10, 3)))
                subset.store_variable(WR_VAR(0, 10, 3), *var);
            else if (const Var* var = c.find(WR_VAR(0, 10, 8)))
                subset.store_variable(WR_VAR(0, 10, 3), *var);
            else if (c.level.ltype1 == 102)
                subset.store_variable_d(WR_VAR(0, 10, 3), (double)c.level.l1 * 9.80665);
            else
                subset.store_variable_undef(WR_VAR(0, 10, 3));

            /* Add wind direction */
            if (const Var* var = c.find(WR_VAR(0, 11, 1)))
                subset.store_variable(WR_VAR(0, 11, 1), *var);
            else
                subset.store_variable_undef(WR_VAR(0, 11, 1));

                /* Add wind speed */
            if (const Var* var = c.find(WR_VAR(0, 11, 2)))
                subset.store_variable(WR_VAR(0, 11, 2), *var);
            else
                subset.store_variable_undef(WR_VAR(0, 11, 2));

            ++group_count;
        }

        subset[rep_count_pos].seti(group_count);

        if (!is_crex)
        {
            int count = subset.append_dpb(WR_VAR(2, 22, 0), subset.size(), WR_VAR(0, 33, 7));
            if (opts.centre != MISSING_INT)
                subset.store_variable_i(WR_VAR(0, 1, 31), opts.centre);
            else
                subset.store_variable_undef(WR_VAR(0, 1, 31));
            if (opts.application != MISSING_INT)
                subset.store_variable_i(WR_VAR(0, 1, 32), opts.application);
            else
                subset.store_variable_undef(WR_VAR(0, 1, 32));
            subset.store_variable_i(WR_VAR(0, 31, 2), count);
        }
    }
};


} // anonymous namespace

void register_temp(TemplateRegistry& r)
{
    r.register_factory(2, "temp", "Temp (autodetect)",
            [](const ExporterOptions& opts, const Messages& msgs) {
                auto msg = Msg::downcast(msgs[0]);

                // Get the type of equipment used
                if (const wreport::Var* var = msg->get(WR_VAR(0, 2, 3), Level(1), Trange::instant()))
                    // Is it a Radar?
                    if (var->enq(0) == 3)
                        return unique_ptr<Template>(new TempRadar(opts, msgs));

                // ECMWF temps use normal replication which cannot do more than 256 levels
                if (msg->data.size() > 260)
                    return unique_ptr<Template>(new TempWMO(opts, msgs));
                const Var* var = msg->get_sonde_tracking_var();
                if (var)
                    return unique_ptr<Template>(new TempWMO(opts, msgs));
                else
                {
                    const wr::TemplateFactory& fac = wr::TemplateRegistry::get("temp-ecmwf");
                    return fac.factory(opts, msgs);
                }
            });
    r.register_factory(2, "temp-ship", "Temp ship (autodetect)",
            [](const ExporterOptions& opts, const Messages& msgs) {
                return unique_ptr<Template>(new TempEcmwfShip(opts, msgs));
            });
    r.register_factory(2, TEMP_WMO_NAME, TEMP_WMO_DESC,
            [](const ExporterOptions& opts, const Messages& msgs) {
                return unique_ptr<Template>(new TempWMO(opts, msgs));
            });
    r.register_factory(2, "temp-ecmwf", "Temp ECMWF (autodetect)",
            [](const ExporterOptions& opts, const Messages& msgs) {
                if (msgs.empty() || Msg::downcast(msgs[0])->type != MessageType::TEMP_SHIP)
                    return unique_ptr<Template>(new TempEcmwfLand(opts, msgs));
                else
                    return unique_ptr<Template>(new TempEcmwfShip(opts, msgs));
            });
    r.register_factory(2, TEMP_ECMWF_LAND_NAME, TEMP_ECMWF_LAND_DESC,
            [](const ExporterOptions& opts, const Messages& msgs) {
                return unique_ptr<Template>(new TempEcmwfLand(opts, msgs));
            });
    r.register_factory(2, TEMP_ECMWF_SHIP_NAME, TEMP_ECMWF_SHIP_DESC,
            [](const ExporterOptions& opts, const Messages& msgs) {
                return unique_ptr<Template>(new TempEcmwfShip(opts, msgs));
            });
    r.register_factory(6, TEMP_RADAR_NAME, TEMP_RADAR_DESC,
            [](const ExporterOptions& opts, const Messages& msgs) {
                return unique_ptr<Template>(new TempRadar(opts, msgs));
            });
    r.register_factory(2, PILOT_WMO_NAME, PILOT_WMO_DESC,
            [](const ExporterOptions& opts, const Messages& msgs) {
                return unique_ptr<Template>(new PilotWMO(opts, msgs));
            });
    r.register_factory(2, PILOT_ECMWF_NAME, PILOT_ECMWF_DESC,
            [](const ExporterOptions& opts, const Messages& msgs) {
                return unique_ptr<Template>(new PilotEcmwf(opts, msgs));
            });
    r.register_factory(2, "pilot", "pilot (autodetect)",
            [](const ExporterOptions& opts, const Messages& msgs) {
                auto msg = Msg::downcast(msgs[0]);
                const Var* var = msg->get_sonde_tracking_var();
                // Try with another one in case the first was just unset
                if (!var) var = msg->get_meas_equip_type_var();
                if (var)
                    return unique_ptr<Template>(new PilotWMO(opts, msgs));
                else
                    return unique_ptr<Template>(new PilotEcmwf(opts, msgs));
            });
}

}
}
}
