/*
 * Copyright (C) 2005--2011  ARPA-SIM <urpsim@smr.arpa.emr.it>
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

#include "common.h"
#include "msg/wr_codec.h"
#include <wreport/bulletin.h>
#include "msg/msgs.h"
#include "msg/context.h"

#warning TODO: remove when done
#include <iostream>

using namespace wreport;
using namespace std;

#define SYNOP_NAME "synop"
#define SYNOP_DESC "Synop (autodetect)"

#define SYNOP_ECMWF_NAME "synop-ecmwf"
#define SYNOP_ECMWF_DESC "Synop ECMWF (autodetect) (0.1)"

#define SYNOP_WMO_NAME "synop-wmo"
#define SYNOP_WMO_DESC "Synop WMO (0.1)"

#define SYNOP_ECMWF_LAND_NAME "synop-ecmwf-land"
#define SYNOP_ECMWF_LAND_DESC "Synop ECMWF land (0.1)"

#define SYNOP_ECMWF_LAND_HIGH_NAME "synop-ecmwf-land-high"
#define SYNOP_ECMWF_LAND_HIGH_DESC "Synop ECMWF land high level station (0.1)"

#define SYNOP_ECMWF_AUTO_NAME "synop-ecmwf-auto"
#define SYNOP_ECMWF_AUTO_DESC "Synop ECMWF land auto (0.3)"

namespace dballe {
namespace msg {
namespace wr {

namespace {

struct Synop : public Template
{
    CommonSynopExporter synop;
    const msg::Context* c_sunshine1;
    const msg::Context* c_sunshine2;
    const msg::Context* c_evapo;

    Synop(const Exporter::Options& opts, const Msgs& msgs)
        : Template(opts, msgs) {}

    void scan_levels()
    {
        c_sunshine1 = NULL;
        c_sunshine2 = NULL;
        c_evapo = NULL;

        // Scan message finding context for the data that follow
        for (std::vector<msg::Context*>::const_iterator i = msg->data.begin();
                i != msg->data.end(); ++i)
        {
            const msg::Context* c = *i;
            synop.scan_context(*c);
            switch (c->level.ltype1)
            {
                case 1:
                    switch (c->trange.pind)
                    {
                        case 1:
                            // msg->set(var, WR_VAR(0, 14, 31), Level(1), Trange(1, 0, time_period));
                            if (c->find(WR_VAR(0, 14, 31)))
                            {
                                if (!c_sunshine1)
                                    c_sunshine1 = c;
                                else if (!c_sunshine2)
                                    c_sunshine2 = c;
                            }

                            // msg->set(var, WR_VAR(0, 13, 33), Level(1), Trange(1, 0, -time_period));
                            if (c->find(WR_VAR(0, 13, 33)))
                                c_evapo = c;
                            break;
                    }
                    break;
            }
        }
    }

    virtual void to_subset(const Msg& msg, wreport::Subset& subset)
    {
        Template::to_subset(msg, subset);
        synop.init(subset);
        scan_levels();
    }
};

// Base template for synops
struct SynopECMWF : public Synop
{
    bool is_crex;
    Varcode prec_code;

    SynopECMWF(const Exporter::Options& opts, const Msgs& msgs)
        : Synop(opts, msgs) {}

    virtual const char* name() const { return SYNOP_ECMWF_NAME; }
    virtual const char* description() const { return SYNOP_ECMWF_DESC; }

    void add_prec()
    {
        const Var* var = NULL;
        switch (prec_code)
        {
            case WR_VAR(0, 13, 23): var = msg->get_tot_prec24_var(); break;
            case WR_VAR(0, 13, 22): var = msg->get_tot_prec12_var(); break;
            case WR_VAR(0, 13, 21): var = msg->get_tot_prec6_var(); break;
            case WR_VAR(0, 13, 20): var = msg->get_tot_prec3_var(); break;
            case WR_VAR(0, 13, 19): var = msg->get_tot_prec1_var(); break;
        }
        if (var)
            subset->store_variable(prec_code, *var);
        else
            subset->store_variable_undef(prec_code);
    }

    virtual void setupBulletin(wreport::Bulletin& bulletin)
    {
        Synop::setupBulletin(bulletin);

        // Use old table for old templates
        if (BufrBulletin* b = dynamic_cast<BufrBulletin*>(&bulletin))
        {
            b->master_table = 13;
        }

        is_crex = dynamic_cast<CrexBulletin*>(&bulletin) != 0;

        // Use the best kind of precipitation found in the message to encode
        prec_code = 0;
        for (Msgs::const_iterator mi = msgs.begin(); prec_code == 0 && mi != msgs.end(); ++mi)
        {
            const Msg& msg = **mi;
            if (msg.get_tot_prec24_var() != NULL)
                prec_code = WR_VAR(0, 13, 23);
            else if (msg.get_tot_prec12_var() != NULL)
                prec_code = WR_VAR(0, 13, 22);
            else if (msg.get_tot_prec6_var() != NULL)
                prec_code = WR_VAR(0, 13, 21);
            else if (msg.get_tot_prec3_var() != NULL)
                prec_code = WR_VAR(0, 13, 20);
            else if (msg.get_tot_prec1_var() != NULL)
                prec_code = WR_VAR(0, 13, 19);
        }
        if (prec_code == 0)
            prec_code = WR_VAR(0, 13, 23);

        bulletin.type = 0;
        bulletin.subtype = 255;
        bulletin.localsubtype = 1;
    }
    virtual void to_subset(const Msg& msg, wreport::Subset& subset)
    {
        Synop::to_subset(msg, subset);
        synop.add_ecmwf_synop_head();
        synop.add_year_to_minute();
        synop.add_latlon_high();
        /* 10 */ add(WR_VAR(0,  7,  1), DBA_MSG_HEIGHT_STATION);
    }
};

struct SynopECMWFLand : public SynopECMWF
{
    SynopECMWFLand(const Exporter::Options& opts, const Msgs& msgs)
        : SynopECMWF(opts, msgs) {}

    virtual const char* name() const { return SYNOP_ECMWF_LAND_NAME; }
    virtual const char* description() const { return SYNOP_ECMWF_LAND_DESC; }

    virtual void setupBulletin(wreport::Bulletin& bulletin)
    {
        SynopECMWF::setupBulletin(bulletin);

        // Data descriptor section
        bulletin.datadesc.clear();
        bulletin.datadesc.push_back(WR_VAR(3,  7,  5));
        bulletin.datadesc.push_back(prec_code);
        bulletin.datadesc.push_back(WR_VAR(0, 13, 13));
        if (!is_crex)
        {
            bulletin.datadesc.push_back(WR_VAR(2, 22,  0));
            bulletin.datadesc.push_back(WR_VAR(1,  1, 49));
            bulletin.datadesc.push_back(WR_VAR(0, 31, 31));
            bulletin.datadesc.push_back(WR_VAR(0,  1, 31));
            bulletin.datadesc.push_back(WR_VAR(0,  1, 32));
            bulletin.datadesc.push_back(WR_VAR(1,  1, 49));
            bulletin.datadesc.push_back(WR_VAR(0, 33,  7));
        }

        bulletin.load_tables();
    }

    virtual void to_subset(const Msg& msg, wreport::Subset& subset)
    {
        SynopECMWF::to_subset(msg, subset);
        synop.add_D02001();
        synop.add_ecmwf_synop_weather();
        /* 24 */ add(WR_VAR(0, 20, 10), DBA_MSG_CLOUD_N);
        /* 25 */ add(WR_VAR(0,  8,  2), WR_VAR(0, 8, 2), Level::cloud(258, 0), Trange::instant());
        /* 26 */ add(WR_VAR(0, 20, 11), DBA_MSG_CLOUD_NH);
        /* 27 */ add(WR_VAR(0, 20, 13), DBA_MSG_CLOUD_HH);
        /* 28 */ add(WR_VAR(0, 20, 12), DBA_MSG_CLOUD_CL);
        /* 29 */ add(WR_VAR(0, 20, 12), DBA_MSG_CLOUD_CM);
        /* 30 */ add(WR_VAR(0, 20, 12), DBA_MSG_CLOUD_CH);
        /* 31 */ add(WR_VAR(0,  8,  2), WR_VAR(0, 8, 2), Level::cloud(259, 1), Trange::instant());
        /* 32 */ add(WR_VAR(0, 20, 11), DBA_MSG_CLOUD_N1);
        /* 33 */ add(WR_VAR(0, 20, 12), DBA_MSG_CLOUD_C1);
        /* 34 */ add(WR_VAR(0, 20, 13), DBA_MSG_CLOUD_H1);
        /* 35 */ add(WR_VAR(0,  8,  2), WR_VAR(0, 8, 2), Level::cloud(259, 2), Trange::instant());
        /* 36 */ add(WR_VAR(0, 20, 11), DBA_MSG_CLOUD_N2);
        /* 37 */ add(WR_VAR(0, 20, 12), DBA_MSG_CLOUD_C2);
        /* 38 */ add(WR_VAR(0, 20, 13), DBA_MSG_CLOUD_H2);
        /* 39 */ add(WR_VAR(0,  8,  2), WR_VAR(0, 8, 2), Level::cloud(259, 3), Trange::instant());
        /* 40 */ add(WR_VAR(0, 20, 11), DBA_MSG_CLOUD_N3);
        /* 41 */ add(WR_VAR(0, 20, 12), DBA_MSG_CLOUD_C3);
        /* 42 */ add(WR_VAR(0, 20, 13), DBA_MSG_CLOUD_H3);
        /* 43 */ add(WR_VAR(0,  8,  2), WR_VAR(0, 8, 2), Level::cloud(259, 4), Trange::instant());
        /* 44 */ add(WR_VAR(0, 20, 11), DBA_MSG_CLOUD_N4);
        /* 45 */ add(WR_VAR(0, 20, 12), DBA_MSG_CLOUD_C4);
        /* 46 */ add(WR_VAR(0, 20, 13), DBA_MSG_CLOUD_H4);
        /* 47 */ add_prec();
        /* 48 */ add(WR_VAR(0, 13, 13), DBA_MSG_TOT_SNOW);

        if (!is_crex)
        {
            subset.append_fixed_dpb(WR_VAR(2, 22, 0), 49);
            if (opts.centre != MISSING_INT)
                subset.store_variable_i(WR_VAR(0, 1, 31), opts.centre);
            else
                subset.store_variable_undef(WR_VAR(0, 1, 31));
            if (opts.application != MISSING_INT)
                subset.store_variable_i(WR_VAR(0, 1, 32), opts.application);
            else
                subset.store_variable_undef(WR_VAR(0, 1, 32));
        }
    }
};

struct SynopECMWFLandHigh : public SynopECMWF
{
    SynopECMWFLandHigh(const Exporter::Options& opts, const Msgs& msgs)
        : SynopECMWF(opts, msgs) {}

    virtual const char* name() const { return SYNOP_ECMWF_LAND_HIGH_NAME; }
    virtual const char* description() const { return SYNOP_ECMWF_LAND_HIGH_DESC; }

    virtual void setupBulletin(wreport::Bulletin& bulletin)
    {
        SynopECMWF::setupBulletin(bulletin);

        // Data descriptor section
        bulletin.datadesc.clear();
        bulletin.datadesc.push_back(WR_VAR(3,  7,  7));
        bulletin.datadesc.push_back(prec_code);
        bulletin.datadesc.push_back(WR_VAR(0, 13, 13));
        if (!is_crex)
        {
            bulletin.datadesc.push_back(WR_VAR(2, 22,  0));
            bulletin.datadesc.push_back(WR_VAR(1,  1, 34));
            bulletin.datadesc.push_back(WR_VAR(0, 31, 31));
            bulletin.datadesc.push_back(WR_VAR(0,  1, 31));
            bulletin.datadesc.push_back(WR_VAR(0,  1, 32));
            bulletin.datadesc.push_back(WR_VAR(1,  1, 34));
            bulletin.datadesc.push_back(WR_VAR(0, 33,  7));
        }

        bulletin.load_tables();
    }

    virtual void to_subset(const Msg& msg, wreport::Subset& subset)
    {
        SynopECMWF::to_subset(msg, subset);

        /* 11 */ add(WR_VAR(0, 10,  4), synop.v_press);
        synop.add_pressure();
        synop.add_geopotential(WR_VAR(0, 10, 3));
        /* 14 */ add(WR_VAR(0, 10, 61), synop.v_pchange3);
        /* 15 */ add(WR_VAR(0, 10, 63), synop.v_ptend);
        synop.add_ecmwf_synop_weather();
        /* 25 */ add(WR_VAR(0, 20, 10), DBA_MSG_CLOUD_N);
        /* 26 */ add(WR_VAR(0,  8,  2), WR_VAR(0, 8, 2), Level::cloud(258, 0), Trange::instant());
        /* 27 */ add(WR_VAR(0, 20, 11), DBA_MSG_CLOUD_NH);
        /* 28 */ add(WR_VAR(0, 20, 13), DBA_MSG_CLOUD_HH);
        /* 29 */ add(WR_VAR(0, 20, 12), DBA_MSG_CLOUD_CL);
        /* 30 */ add(WR_VAR(0, 20, 12), DBA_MSG_CLOUD_CM);
        /* 31 */ add(WR_VAR(0, 20, 12), DBA_MSG_CLOUD_CH);
        /* 32 */ add_prec();
        /* 33 */ add(WR_VAR(0, 13, 13), DBA_MSG_TOT_SNOW);

        if (!is_crex)
        {
            subset.append_fixed_dpb(WR_VAR(2, 22, 0), 34);
            if (opts.centre != MISSING_INT)
                subset.store_variable_i(WR_VAR(0, 1, 31), opts.centre);
            else
                subset.store_variable_undef(WR_VAR(0, 1, 31));
            if (opts.application != MISSING_INT)
                subset.store_variable_i(WR_VAR(0, 1, 32), opts.application);
            else
                subset.store_variable_undef(WR_VAR(0, 1, 32));
        }
    }
};

// Same as SynopECMWFLandHigh but just with a different local subtype
struct SynopECMWFAuto : public SynopECMWFLand
{
    SynopECMWFAuto(const Exporter::Options& opts, const Msgs& msgs)
        : SynopECMWFLand(opts, msgs) {}

    virtual const char* name() const { return SYNOP_ECMWF_AUTO_NAME; }
    virtual const char* description() const { return SYNOP_ECMWF_AUTO_DESC; }

    virtual void setupBulletin(wreport::Bulletin& bulletin)
    {
        SynopECMWFLand::setupBulletin(bulletin);

        bulletin.localsubtype = 3;
    }
};

struct SynopWMO : public Synop
{
    bool is_crex;
    // Keep track of the bulletin being written, since we amend its headers as
    // we process its subsets
    Bulletin* cur_bulletin;

    SynopWMO(const Exporter::Options& opts, const Msgs& msgs)
        : Synop(opts, msgs), cur_bulletin(0) {}

    virtual const char* name() const { return SYNOP_WMO_NAME; }
    virtual const char* description() const { return SYNOP_WMO_DESC; }

    virtual void setupBulletin(wreport::Bulletin& bulletin)
    {
        Synop::setupBulletin(bulletin);

        is_crex = dynamic_cast<CrexBulletin*>(&bulletin) != 0;

        bulletin.type = 0;
        bulletin.subtype = 255;
        bulletin.localsubtype = 255;

        // Data descriptor section
        bulletin.datadesc.clear();
        bulletin.datadesc.push_back(WR_VAR(3, 7, 80));
        bulletin.load_tables();

        cur_bulletin = &bulletin;
    }

    // D02036  Clouds with bases below station level
    void do_D02036(const Msg& msg, wreport::Subset& subset)
    {
        // Number of individual cloud layers or masses
        int max_cloud_group = 0;
        for (int i = 1; ; ++i)
        {
            if (msg.find_context(Level::cloud(263, i), Trange::instant()))
            {
                max_cloud_group = i;
            } else if (i > 4)
                break;
        }
        subset.store_variable_i(WR_VAR(0, 1, 31), max_cloud_group);
        for (int i = 1; i <= max_cloud_group; ++i)
        {
            add(WR_VAR(0,  8,  2), WR_VAR(0, 8, 2), Level::cloud(263, i), Trange::instant());
            const msg::Context* c = msg.find_context(Level::cloud(263, i), Trange::instant());
            add(WR_VAR(0, 20, 11), c);
            add(WR_VAR(0, 20, 12), c);
            add(WR_VAR(0, 20, 14), c);
            add(WR_VAR(0, 20, 17), c);
        }
    }

    // D02047  Direction of cloud drift
    void do_D02047(const Msg& msg, wreport::Subset& subset)
    {
        // D02047  Direction of cloud drift
        for (int i = 1; i <= 3; ++i)
        {
            if (const msg::Context* c = msg.find_context(Level::cloud(260, i), Trange::instant()))
            {
                if (const Var* var = c->find(WR_VAR(0,  8,  2)))
                    subset.store_variable(*var);
                else
                    subset.store_variable_undef(WR_VAR(0,  8,  2));

                if (const Var* var = c->find(WR_VAR(0, 20, 54)))
                    subset.store_variable(*var);
                else
                    subset.store_variable_undef(WR_VAR(0, 20, 54));
            } else {
                subset.store_variable_undef(WR_VAR(0,  8,  2));
                subset.store_variable_undef(WR_VAR(0, 20, 54));
            }
        }
    }

    // D02048  Direction and elevation of cloud
    void do_D02048(const Msg& msg, wreport::Subset& subset)
    {
        if (const msg::Context* c = msg.find_context(Level::cloud(262, 0), Trange::instant()))
        {
            if (const Var* var = c->find(WR_VAR(0,  5, 21)))
                subset.store_variable(*var);
            else
                subset.store_variable_undef(WR_VAR(0,  5, 21));

            if (const Var* var = c->find(WR_VAR(0,  7, 21)))
                subset.store_variable(*var);
            else
                subset.store_variable_undef(WR_VAR(0,  7, 21));

            if (const Var* var = c->find(WR_VAR(0, 20, 12)))
                subset.store_variable(*var);
            else
                subset.store_variable_undef(WR_VAR(0, 20, 12));
        } else {
            subset.store_variable_undef(WR_VAR(0,  5, 21));
            subset.store_variable_undef(WR_VAR(0,  7, 21));
            subset.store_variable_undef(WR_VAR(0, 20, 12));
        }
        subset.store_variable_undef(WR_VAR(0,  5, 21));
        subset.store_variable_undef(WR_VAR(0,  7, 21));
    }

    // D02037  State of ground, snow depth, ground minimum temperature
    void do_D02037(const Msg& msg, wreport::Subset& subset)
    {
        add(WR_VAR(0, 20,  62), DBA_MSG_STATE_GROUND);
        add(WR_VAR(0, 13,  13), DBA_MSG_TOT_SNOW);
        if (const Var* var = msg.find(WR_VAR(0, 12, 121), Level(1), Trange(3, 0, 43200)))
            subset.store_variable(WR_VAR(0, 12, 113), *var);
        else
            subset.store_variable_undef(WR_VAR(0, 12, 113));
    }

    // D02043  Basic synoptic "period" data
    void do_D02043(int hour)
    {
        synop.add_D02038();

        //   Sunshine data (form 1 hour and 24 hour period)
        if (c_sunshine1)
        {
            subset->store_variable_d(WR_VAR(0,  4, 24), -c_sunshine1->trange.p2 / 3600);
            if (const Var* var = c_sunshine1->find(WR_VAR(0, 14, 31)))
                subset->store_variable(*var);
            else
                subset->store_variable_undef(WR_VAR(0, 14, 31));
        } else {
            subset->store_variable_undef(WR_VAR(0,  4, 24));
            subset->store_variable_undef(WR_VAR(0, 14, 31));
        }
        if (c_sunshine2)
        {
            subset->store_variable_d(WR_VAR(0,  4, 24), -c_sunshine2->trange.p2 / 3600);
            if (const Var* var = c_sunshine2->find(WR_VAR(0, 14, 31)))
                subset->store_variable(*var);
            else
                subset->store_variable_undef(WR_VAR(0, 14, 31));
        } else {
            subset->store_variable_undef(WR_VAR(0,  4, 24));
            subset->store_variable_undef(WR_VAR(0, 14, 31));
        }

        //   Precipitation measurement
        synop.add_D02040();

        // Extreme temperature data
        synop.add_D02041();

        //   Wind data
        synop.add_D02042();
        subset->store_variable_undef(WR_VAR(0,  7, 32));
    }

    virtual void to_subset(const Msg& msg, wreport::Subset& subset)
    {
        Synop::to_subset(msg, subset);

        // Set subtype based on hour. If we have heterogeneous subsets, keep
        // the lowest of the constraints
        int hour = synop.get_hour();
        if ((hour % 6) == 0)
            // 002 at main synoptic times 00, 06, 12, 18 UTC,
            cur_bulletin->subtype = cur_bulletin->subtype == 255 ? 2 : cur_bulletin->subtype;
        else if ((hour % 3 == 0))
            // 001 at intermediate synoptic times 03, 09, 15, 21 UTC,
            cur_bulletin->subtype = cur_bulletin->subtype != 0 ? 1 : cur_bulletin->subtype;
        else
            // 000 at observation times 01, 02, 04, 05, 07, 08, 10, 11, 13, 14, 16, 17, 19, 20, 22 and 23 UTC.
            cur_bulletin->subtype = 0;

        // Fixed surface station identification, time, horizontal and vertical
        // coordinates
        synop.add_D01090();

        // D02031  Pressure data
        synop.add_D02031();

        // D02035  Basic synoptic "instantaneous" data
        synop.add_D02035();
        // D02036  Clouds with bases below station level
        do_D02036(msg, subset);
        // D02047  Direction of cloud drift
        do_D02047(msg, subset);
        // B08002
        subset.store_variable_undef(WR_VAR(0, 8, 2));
        // D02048  Direction and elevation of cloud
        do_D02048(msg, subset);
        // D02037  State of ground, snow depth, ground minimum temperature
        do_D02037(msg, subset);
        // D02043  Basic synoptic "period" data
        do_D02043(hour);

        // D02044  Evaporation data
        if (c_evapo)
        {
            if (c_evapo->trange.p1 == 0)
                subset.store_variable_d(WR_VAR(0,  4, 24), -c_evapo->trange.p2/3600);
            else
                subset.store_variable_undef(WR_VAR(0,  4, 24));
            add(WR_VAR(0,  2,  4), WR_VAR(0,  2,  4), Level(1), Trange::instant());
        } else {
            subset.store_variable_undef(WR_VAR(0,  4, 24));
            add(WR_VAR(0,  2,  4), WR_VAR(0,  2,  4), Level(1), Trange::instant());
        }
        add(WR_VAR(0, 13, 33), c_evapo);

#warning TODO
        // D02045  Radiation data (1 hour period)
        subset.store_variable_undef(WR_VAR(0,  4, 24)); // TODO
        subset.store_variable_undef(WR_VAR(0, 14,  2)); // TODO
        subset.store_variable_undef(WR_VAR(0, 14,  4)); // TODO
        subset.store_variable_undef(WR_VAR(0, 14, 16)); // TODO
        subset.store_variable_undef(WR_VAR(0, 14, 28)); // TODO
        subset.store_variable_undef(WR_VAR(0, 14, 29)); // TODO
        subset.store_variable_undef(WR_VAR(0, 14, 30)); // TODO
        // D02045  Radiation data (24 hour period)
        subset.store_variable_undef(WR_VAR(0,  4, 24)); // TODO
        subset.store_variable_undef(WR_VAR(0, 14,  2)); // TODO
        subset.store_variable_undef(WR_VAR(0, 14,  4)); // TODO
        subset.store_variable_undef(WR_VAR(0, 14, 16)); // TODO
        subset.store_variable_undef(WR_VAR(0, 14, 28)); // TODO
        subset.store_variable_undef(WR_VAR(0, 14, 29)); // TODO
        subset.store_variable_undef(WR_VAR(0, 14, 30)); // TODO
        // D02046  Temperature change
        subset.store_variable_undef(WR_VAR(0,  4, 24)); // TODO
        subset.store_variable_undef(WR_VAR(0,  4, 24)); // TODO
        subset.store_variable_undef(WR_VAR(0, 12, 49)); // TODO
    }
};


struct SynopWMOFactory : public virtual TemplateFactory
{
    SynopWMOFactory() { name = SYNOP_WMO_NAME; description = SYNOP_WMO_DESC; }

    std::unique_ptr<Template> make(const Exporter::Options& opts, const Msgs& msgs) const
    {
        // Scan msgs and pick the right one
        return unique_ptr<Template>(new SynopWMO(opts, msgs));
    }
};

struct SynopECMWFLandFactory : public virtual TemplateFactory
{
    SynopECMWFLandFactory() { name = SYNOP_ECMWF_LAND_NAME; description = SYNOP_ECMWF_LAND_DESC; }

    std::unique_ptr<Template> make(const Exporter::Options& opts, const Msgs& msgs) const
    {
        return unique_ptr<Template>(new SynopECMWFLand(opts, msgs));
    }
};

struct SynopECMWFLandHighFactory : public virtual TemplateFactory
{
    SynopECMWFLandHighFactory() { name = SYNOP_ECMWF_LAND_HIGH_NAME; description = SYNOP_ECMWF_LAND_HIGH_DESC; }

    std::unique_ptr<Template> make(const Exporter::Options& opts, const Msgs& msgs) const
    {
        return unique_ptr<Template>(new SynopECMWFLandHigh(opts, msgs));
    }
};

struct SynopECMWFAutoFactory : public virtual TemplateFactory
{
    SynopECMWFAutoFactory() { name = SYNOP_ECMWF_AUTO_NAME; description = SYNOP_ECMWF_AUTO_DESC; }

    std::unique_ptr<Template> make(const Exporter::Options& opts, const Msgs& msgs) const
    {
        return unique_ptr<Template>(new SynopECMWFAuto(opts, msgs));
    }
};

struct SynopECMWFFactory : public virtual TemplateFactory
{
    SynopECMWFFactory() { name = SYNOP_ECMWF_NAME; description = SYNOP_ECMWF_DESC; }

    std::unique_ptr<Template> make(const Exporter::Options& opts, const Msgs& msgs) const
    {
        const Msg& msg = *msgs[0];
        const Var* var = msg.get_st_type_var();
        if (var != NULL && var->enqi() == 0)
            return unique_ptr<Template>(new SynopECMWFAuto(opts, msgs));

        // If it has a geopotential, it's a land high station
        for (std::vector<msg::Context*>::const_iterator i = msg.data.begin();
                i != msg.data.end(); ++i)
            if ((*i)->level.ltype1 == 100)
                if (const Var* v = (*i)->find(WR_VAR(0, 10, 8)))
                    return unique_ptr<Template>(new SynopECMWFLandHigh(opts, msgs));

        return unique_ptr<Template>(new SynopECMWFLand(opts, msgs));
    }
};

struct SynopFactory : public SynopECMWFFactory, SynopWMOFactory
{
    SynopFactory() { name = SYNOP_NAME; description = SYNOP_DESC; }

    std::unique_ptr<Template> make(const Exporter::Options& opts, const Msgs& msgs) const
    {
        const Msg& msg = *msgs[0];
        const Var* var = msg.get_st_name_var();
        if (var)
            return SynopWMOFactory::make(opts, msgs);
        else
            return SynopECMWFFactory::make(opts, msgs);
    }
};


} // anonymous namespace

void register_synop(TemplateRegistry& r)
{
static const TemplateFactory* synop = NULL;
static const TemplateFactory* synopwmo = NULL;
static const TemplateFactory* synopecmwf = NULL;
static const TemplateFactory* synopecmwfland = NULL;
static const TemplateFactory* synopecmwflandhigh = NULL;
static const TemplateFactory* synopecmwfauto = NULL;

    if (!synop) synop = new SynopFactory;
    if (!synopwmo) synopwmo = new SynopWMOFactory;
    if (!synopecmwf) synopecmwf = new SynopECMWFFactory;
    if (!synopecmwfland) synopecmwfland = new SynopECMWFLandFactory;
    if (!synopecmwflandhigh) synopecmwflandhigh = new SynopECMWFLandHighFactory;
    if (!synopecmwfauto) synopecmwfauto = new SynopECMWFAutoFactory;

    r.register_factory(synop);
    r.register_factory(synopwmo);
    r.register_factory(synopecmwf);
    r.register_factory(synopecmwfland);
    r.register_factory(synopecmwflandhigh);
    r.register_factory(synopecmwfauto);
}

}
}
}
/* vim:set ts=4 sw=4: */
