/*
 * dballe/wr_codec - BUFR/CREX import and export
 *
 * Copyright (C) 2005--2010  ARPA-SIM <urpsim@smr.arpa.emr.it>
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

#include "wr_codec.h"
#include "msgs.h"
#include <wreport/bulletin.h>
#include "wr_importers/base.h"
//#include <dballe/core/verbose.h>

using namespace wreport;
using namespace std;

namespace dballe {
namespace msg {

WRImporter::WRImporter(const Options& opts)
	: Importer(opts) {}

BufrImporter::BufrImporter(const Options& opts)
	: WRImporter(opts) {}
BufrImporter::~BufrImporter() {}

void BufrImporter::from_rawmsg(const Rawmsg& msg, Msgs& msgs) const
{
	BufrBulletin bulletin;
	bulletin.decode(msg);
	from_bulletin(bulletin, msgs);
}

CrexImporter::CrexImporter(const Options& opts)
	: WRImporter(opts) {}
CrexImporter::~CrexImporter() {}

void CrexImporter::from_rawmsg(const Rawmsg& msg, Msgs& msgs) const
{
	CrexBulletin bulletin;
	bulletin.decode(msg);
	from_bulletin(bulletin, msgs);
}

void WRImporter::from_bulletin(const wreport::Bulletin& msg, Msgs& msgs) const
{
	// Infer the right importer
	std::auto_ptr<wr::Importer> importer;
	switch (msg.type)
	{
		case 0:
		case 1:
			if (msg.localsubtype == 140)
				importer = wr::Importer::createMetar(opts);
			else
				importer = wr::Importer::createSynop(opts);
			break;
		case 2:
			if (msg.localsubtype == 91 || msg.localsubtype == 92)
				importer = wr::Importer::createPilot(opts);
			else
				importer = wr::Importer::createTemp(opts);
			break;
		case 3: importer = wr::Importer::createSat(opts); break;
		case 4: importer = wr::Importer::createFlight(opts); break;
		case 8: importer = wr::Importer::createPollution(opts); break;
		default: importer = wr::Importer::createGeneric(opts); break;
	}

	MsgType type = importer->scanType(msg);
	for (unsigned i = 0; i < msg.subsets.size(); ++i)
	{
		std::auto_ptr<Msg> newmsg(new Msg);
		newmsg->type = type;
		importer->import(msg.subsets[i], *newmsg);
		msgs.acquire(newmsg);
	}
}


WRExporter::WRExporter(const Options& opts)
	: Exporter(opts) {}

BufrExporter::BufrExporter(const Options& opts)
	: WRExporter(opts) {}
BufrExporter::~BufrExporter() {}

void BufrExporter::to_rawmsg(const Msgs& msgs, Rawmsg& msg) const
{
	BufrBulletin bulletin;
	to_bulletin(msgs, bulletin);
	bulletin.encode(msg);
}

CrexExporter::CrexExporter(const Options& opts)
	: WRExporter(opts) {}
CrexExporter::~CrexExporter() {}

void CrexExporter::to_rawmsg(const Msgs& msgs, Rawmsg& msg) const
{
	CrexBulletin bulletin;
	to_bulletin(msgs, bulletin);
	bulletin.encode(msg);
}

void WRExporter::to_bulletin(const Msgs& msgs, wreport::Bulletin& bulletin) const
{
	if (msgs.empty())
		throw error_consistency("trying to export an empty message set");

	// Select initial template name
	string tpl = opts.template_name;
	if (tpl.empty())
    {
        switch (msgs[0]->type)
        {
            case MSG_TEMP_SHIP: tpl = "temp-ship"; break;
            default: tpl = msg_type_name(msgs[0]->type); break;
        }
    }

	// Get template factory
	const wr::TemplateFactory& fac = wr::TemplateRegistry::get(tpl);
	std::auto_ptr<wr::Template> encoder = fac.make(opts, msgs);
    // fprintf(stderr, "Encoding with template %s\n", encoder->name());
	encoder->to_bulletin(bulletin);
}

namespace wr {

extern void register_synop(TemplateRegistry&);
extern void register_ship(TemplateRegistry&);
extern void register_buoy(TemplateRegistry&);
extern void register_metar(TemplateRegistry&);
extern void register_temp(TemplateRegistry&);
extern void register_pilot(TemplateRegistry&);
extern void register_flight(TemplateRegistry&);
extern void register_generic(TemplateRegistry&);
extern void register_pollution(TemplateRegistry&);

static TemplateRegistry* registry = NULL;
const TemplateRegistry& TemplateRegistry::get()
{
    if (!registry)
    {
        registry = new TemplateRegistry;
        
        // Populate it
        register_synop(*registry);
        register_ship(*registry);
        register_buoy(*registry);
        register_metar(*registry);
        register_temp(*registry);
        register_pilot(*registry);
        register_flight(*registry);
        register_generic(*registry);
        register_pollution(*registry);

        // registry->insert("synop", ...)
        // registry->insert("synop-high", ...)
        // registry->insert("wmo-synop", ...)
        // registry->insert("wmo-synop-high", ...)
        // registry->insert("ecmwf-synop", ...)
        // registry->insert("ecmwf-synop-high", ...)
    }
    return *registry;
}

const TemplateFactory& TemplateRegistry::get(const std::string& name)
{
    const TemplateRegistry& tr = get();
    TemplateRegistry::const_iterator i = tr.find(name);
    if (i == tr.end())
        error_notfound::throwf("requested export template %s which does not exist", name.c_str());
    return *(i->second);
}

void TemplateRegistry::register_factory(const TemplateFactory* fac)
{
    insert(make_pair(fac->name, fac));
}

void Template::to_bulletin(wreport::Bulletin& bulletin)
{
    setupBulletin(bulletin);

    for (unsigned i = 0; i < msgs.size(); ++i)
    {
        Subset& s = bulletin.obtain_subset(i);
        to_subset(*msgs[i], s);
    }
}

void Template::setupBulletin(wreport::Bulletin& bulletin)
{
    // Get reference time from first msg in the set
    // If not found, use current time.
    const Msg& msg = *msgs[0];
    bool has_date = true;
    if (const Var* v = msg.get_year_var())
        bulletin.rep_year = v->enqi();
    else
        has_date = false;
    if (const Var* v = msg.get_month_var())
        bulletin.rep_month = v->enqi();
    else
        has_date = false;
    if (const Var* v = msg.get_day_var())
        bulletin.rep_day = v->enqi();
    else
        has_date = false;
    if (const Var* v = msg.get_hour_var())
        bulletin.rep_hour = v->enqi();
    else
        bulletin.rep_hour = 0;
    if (const Var* v = msg.get_minute_var())
        bulletin.rep_minute = v->enqi();
    else
        bulletin.rep_minute = 0;
    if (const Var* v = msg.get_second_var())
        bulletin.rep_second = v->enqi();
    else
        bulletin.rep_second = 0;
    if (!has_date)
    {
        // use today
        time_t tnow = time(NULL);
        struct tm now;
        gmtime_r(&tnow, &now);
        bulletin.rep_year = now.tm_year + 1900;
        bulletin.rep_month = now.tm_mon + 1;
        bulletin.rep_day = now.tm_mday;
        bulletin.rep_hour = now.tm_hour;
        bulletin.rep_minute = now.tm_min;
        bulletin.rep_second = now.tm_sec;
    }

    if (BufrBulletin* b = dynamic_cast<BufrBulletin*>(&bulletin))
    {
        // Take from opts
        b->centre = opts.centre != MISSING_INT ? opts.centre : 255;
        b->subcentre = opts.subcentre != MISSING_INT ? opts.subcentre : 255;
        b->master_table = 14;
        b->local_table = 0;
        b->compression = 0;
        b->update_sequence_number = 0;
	b->edition = 4;
    }
    if (CrexBulletin* b = dynamic_cast<CrexBulletin*>(&bulletin))
    {
        b->master_table = 0;
        b->table = 3;
        b->has_check_digit = 0;
	b->edition = 2;
    }
}

void Template::to_subset(const Msg& msg, wreport::Subset& subset)
{
    this->msg = &msg;
    this->subset = &subset;
}

} // namespace wr


} // namespace msg
} // namespace dballe

/* vim:set ts=4 sw=4: */