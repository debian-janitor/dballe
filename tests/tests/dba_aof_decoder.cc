#include <tests/test-utils.h>
#include <dballe/aof/decoder.h>
#include <dballe/dba_file.h>

namespace tut {
using namespace tut_dballe;

struct dba_aof_decoder_shar
{
	dba_aof_decoder_shar()
	{
	}

	~dba_aof_decoder_shar()
	{
	}
};
TESTGRP(dba_aof_decoder);

// Test simple decoding
template<> template<>
void to::test<1>()
{
	const char* files[] = {
		"aof/obs1-11.0.aof",
		"aof/obs1-14.63.aof",
		"aof/obs1-21.1.aof",
		"aof/obs1-24.2104.aof",
		"aof/obs1-24.34.aof",
		"aof/obs2-144.2198.aof",
		"aof/obs2-244.0.aof",
		"aof/obs2-244.1.aof",
		"aof/obs4-165.2027.aof",
		"aof/obs5-35.61.aof",
		"aof/obs5-36.30.aof",
		"aof/obs6-32.1573.aof",
		"aof/obs6-32.0.aof",
		NULL,
	};
	dba_rawmsg raw;

	/* Initialise decoding objects */
	CHECKED(dba_rawmsg_create(&raw));

	for (size_t i = 0; files[i] != NULL; i++)
	{
		test_tag(files[i]);

		dba_file file;
		int found;

		/* Create the file reader */
		CHECKED(dba_file_create(&file, AOF, files[i], "r"));

		/* Read the file header */
		/* CHECKED(aof_file_read_header(file, 0, 0)); */

		/* Read the data from file */
		CHECKED(dba_file_read_raw(file, raw, &found));
		gen_ensure_equals(found, 1);

		/* Parse it */
		dba_msg msg = NULL;
		CHECKED(aof_decoder_decode(raw, &msg));
		gen_ensure(msg != NULL);

		dba_msg_delete(msg);
		dba_file_delete(file);
	}
	test_untag();

	dba_rawmsg_delete(raw);
}

void normalise_encoding_quirks(dba_msg amsg, dba_msg bmsg)
{
	// Recode BUFR attributes to match the AOF 2-bit values
	for (int i = 0; i < bmsg->data_count; i++)
	{
		dba_msg_level lev = bmsg->data[i];
		for (int j = 0; j < lev->data_count; j++)
		{
			dba_msg_datum dat = lev->data[j];
			dba_var_attr_iterator iter = dba_var_attr_iterate(dat->var);
			for (; iter != NULL; iter = dba_var_attr_iterator_next(iter))
			{
				dba_var attr = dba_var_attr_iterator_attr(iter);
				if (dba_var_code(attr) == DBA_VAR(0, 33, 7))
				{
					int val;
					CHECKED(dba_var_enqi(attr, &val));
					// Recode val using one of the value in the 4 steps of AOF
					if (val > 75)
						val = 76;
					else if (val > 50)
						val = 51;
					else if (val > 25)
						val = 26;
					else
						val = 0;
					CHECKED(dba_var_seti(attr, val));
				}
			}
		}
	}
	
	dba_var var;

	if ((var = dba_msg_get_block_var(bmsg)) != NULL)
		dba_var_clear_attrs(var);
	if ((var = dba_msg_get_station_var(bmsg)) != NULL)
		dba_var_clear_attrs(var);
	if ((var = dba_msg_get_st_type_var(bmsg)) != NULL)
		dba_var_clear_attrs(var);
	if ((var = dba_msg_get_ident_var(bmsg)) != NULL)
		dba_var_clear_attrs(var);
	if ((var = dba_msg_get_flight_phase_var(bmsg)) != NULL)
		dba_var_clear_attrs(var);

	if ((var = dba_msg_get_cloud_cl_var(bmsg)) != NULL &&
			strtoul(dba_var_value(var), NULL, 0) == 62 &&
			dba_msg_get_cloud_cl_var(amsg) == NULL)
		CHECKED(dba_msg_set_cloud_cl_var(amsg, var));

	if ((var = dba_msg_get_cloud_cm_var(bmsg)) != NULL &&
			strtoul(dba_var_value(var), NULL, 0) == 61 &&
			dba_msg_get_cloud_cm_var(amsg) == NULL)
		CHECKED(dba_msg_set_cloud_cm_var(amsg, var));

	if ((var = dba_msg_get_cloud_ch_var(bmsg)) != NULL &&
			strtoul(dba_var_value(var), NULL, 0) == 60 &&
			dba_msg_get_cloud_ch_var(amsg) == NULL)
		CHECKED(dba_msg_set_cloud_ch_var(amsg, var));

	if ((var = dba_msg_get_height_anem_var(bmsg)) != NULL &&
			dba_msg_get_height_anem_var(amsg) == NULL)
		CHECKED(dba_msg_set_height_anem_var(amsg, var));

	if ((var = dba_msg_get_navsys_var(bmsg)) != NULL &&
			dba_msg_get_navsys_var(amsg) == NULL)
		CHECKED(dba_msg_set_navsys_var(amsg, var));

	// In AOF, only synops and ships can encode direction and speed
	if (amsg->type != MSG_SHIP && amsg->type != MSG_SYNOP)
	{
		if ((var = dba_msg_get_st_dir_var(bmsg)) != NULL &&
				dba_msg_get_st_dir_var(amsg) == NULL)
			CHECKED(dba_msg_set_st_dir_var(amsg, var));

		if ((var = dba_msg_get_st_speed_var(bmsg)) != NULL &&
				dba_msg_get_st_speed_var(amsg) == NULL)
			CHECKED(dba_msg_set_st_speed_var(amsg, var));
	}

	if ((var = dba_msg_get_press_tend_var(bmsg)) != NULL &&
			dba_msg_get_press_tend_var(amsg) == NULL)
		CHECKED(dba_msg_set_press_tend_var(amsg, var));

	// AOF AMDAR has pressure indication, BUFR AMDAR has only height
	if (amsg->type == MSG_AMDAR)
	{
		dba_var p = dba_msg_get_flight_press_var(amsg);
#if 0
		dba_var h = dba_msg_get_height_var(bmsg);
		if (p && h)
		{
			double press, height;
			CHECKED(dba_var_enqd(p, &press));
			CHECKED(dba_var_enqd(h, &height));
			dba_msg_level l = dba_msg_find_level(bmsg, 103, (int)height, 0);
			if (l)
			{
				l->ltype = 100;
				l->l1 = (int)(press/100);
				CHECKED(dba_msg_set_flight_press_var(bmsg, p));
			}
		}
#endif
		if (p)
			CHECKED(dba_msg_set_flight_press_var(bmsg, p));
	}

	if (amsg->type == MSG_TEMP)
	{
		if ((var = dba_msg_get_sonde_type_var(bmsg)) != NULL &&
				dba_msg_get_sonde_type_var(amsg) == NULL)
		CHECKED(dba_msg_set_sonde_type_var(amsg, var));

		if ((var = dba_msg_get_sonde_method_var(bmsg)) != NULL &&
				dba_msg_get_sonde_method_var(amsg) == NULL)
		CHECKED(dba_msg_set_sonde_method_var(amsg, var));
	}

	if (amsg->type == MSG_TEMP_SHIP)
	{
		if ((var = dba_msg_get_height_var(bmsg)) != NULL &&
				dba_msg_get_height_var(amsg) == NULL)
		CHECKED(dba_msg_set_height_var(amsg, var));
	}

 	if (amsg->type == MSG_TEMP || amsg->type == MSG_TEMP_SHIP)
	{
		if ((var = dba_msg_get_cloud_n_var(bmsg)) != NULL &&
				dba_msg_get_cloud_n_var(amsg) == NULL)
			CHECKED(dba_msg_set_cloud_n_var(amsg, var));

		if ((var = dba_msg_get_cloud_nh_var(bmsg)) != NULL &&
				dba_msg_get_cloud_nh_var(amsg) == NULL)
			CHECKED(dba_msg_set_cloud_nh_var(amsg, var));

		if ((var = dba_msg_get_cloud_hh_var(bmsg)) != NULL &&
				dba_msg_get_cloud_hh_var(amsg) == NULL)
			CHECKED(dba_msg_set_cloud_hh_var(amsg, var));

		if ((var = dba_msg_get_cloud_cl_var(bmsg)) != NULL &&
				dba_msg_get_cloud_cl_var(amsg) == NULL)
			CHECKED(dba_msg_set_cloud_cl_var(amsg, var));

		if ((var = dba_msg_get_cloud_cm_var(bmsg)) != NULL &&
				dba_msg_get_cloud_cm_var(amsg) == NULL)
			CHECKED(dba_msg_set_cloud_cm_var(amsg, var));

		if ((var = dba_msg_get_cloud_ch_var(bmsg)) != NULL &&
				dba_msg_get_cloud_ch_var(amsg) == NULL)
			CHECKED(dba_msg_set_cloud_ch_var(amsg, var));
	}

	// Remove attributes from all vertical sounding significances
	for (int i = 0; i < bmsg->data_count; i++)
	{
		dba_msg_level lev = bmsg->data[i];
		for (int j = 0; j < lev->data_count; j++)
		{
			dba_msg_datum dat = lev->data[j];
			if (dba_var_code(dat->var) == DBA_VAR(0, 8, 1))
				dba_var_clear_attrs(dat->var);
		}
	}
}

// Reencode to BUFR and compare
template<> template<>
void to::test<2>()
{
	const char* files[] = {
		"aof/obs1-11.0.aof",
		"aof/obs1-14.63.aof",
		"aof/obs1-21.1.aof",
		"aof/obs1-24.2104.aof",
		"aof/obs1-24.34.aof",
		"aof/obs2-144.2198.aof",
		"aof/obs2-244.0.aof",
		"aof/obs2-244.1.aof",
		"aof/obs4-165.2027.aof",
		"aof/obs5-35.61.aof",
		"aof/obs5-36.30.aof",
		"aof/obs6-32.1573.aof",
		"aof/obs6-32.0.aof",
		NULL,
	};

	for (size_t i = 0; files[i] != NULL; i++)
	{
		test_tag(files[i]);

		dba_msg amsg = read_test_msg(files[i], AOF);
		
		dba_rawmsg raw;
		CHECKED(dba_marshal_encode(amsg, BUFR, &raw));

		dba_msg bmsg;
		CHECKED(dba_marshal_decode(raw, &bmsg));

		normalise_encoding_quirks(amsg, bmsg);

		// Compare the two dba_msg
		int diffs = 0;
		dba_msg_diff(amsg, bmsg, &diffs, stderr);
		if (diffs) track_different_msgs(amsg, bmsg, "aof-recoded");
		gen_ensure_equals(diffs, 0);

		dba_msg_delete(amsg);
		dba_msg_delete(bmsg);
		dba_rawmsg_delete(raw);
	}
	test_untag();
}


// Compare decoding results with BUFR sample data
template<> template<>
void to::test<3>()
{
	string files[] = {
		"aof/obs1-14.63",		// OK
		"aof/obs1-21.1",		// OK
		"aof/obs1-24.2104",		// OK
//		"aof/obs1-24.34",		// Data are in fact slightly different
//		"aof/obs2-144.2198",	// Data have different precision in BUFR and AOF, but otherwise match
//		"aof/obs2-244.0",		// BUFR counterpart missing for this message
		"aof/obs4-165.2027",	// OK
		"aof/obs5-35.61",		// OK
		"aof/obs5-36.30",		// OK
		"aof/obs6-32.1573",		// OK
//		"aof/obs6-32.0",		// BUFR conterpart missing for this message
		"",
	};

	for (size_t i = 0; !files[i].empty(); i++)
	{
		test_tag(files[i]);

		dba_msg amsg = read_test_msg((files[i] + ".aof").c_str(), AOF);
		dba_msg bmsg = read_test_msg((files[i] + ".bufr").c_str(), BUFR);
		normalise_encoding_quirks(amsg, bmsg);

		// Compare the two dba_msg
		int diffs = 0;
		dba_msg_diff(amsg, bmsg, &diffs, stderr);
		if (diffs) track_different_msgs(amsg, bmsg, "aof");
		gen_ensure_equals(diffs, 0);

		dba_msg_delete(amsg);
		dba_msg_delete(bmsg);
	}
	test_untag();
}

#if 0
	CHECKED(dba_aof_decoder_start(decoder, "test-aof-01"));
	while (1)
	{
		/* Read all the records */
		dba_err err = dba_aof_decoder_next(decoder, rec);
		if (err == DBA_ERROR && dba_error_get_code() == DBA_ERR_NOTFOUND)
			break;
		if (err) print_dba_error();
		fail_unless(err == DBA_OK);
		//dump_rec(rec);
	}
		
	CHECKED(dba_aof_decoder_start(decoder, "test-aof-02"));
	while (1)
	{
		/* Read all the records */
		dba_err err = dba_aof_decoder_next(decoder, rec);
		if (err == DBA_ERROR && dba_error_get_code() == DBA_ERR_NOTFOUND)
			break;
		if (err) print_dba_error();
		fail_unless(err == DBA_OK);
	}

	CHECKED(dba_aof_decoder_start(decoder, "test-aof-03"));
	while (1)
	{
		/* Read all the records */
		dba_err err = dba_aof_decoder_next(decoder, rec);
		if (err == DBA_ERROR && dba_error_get_code() == DBA_ERR_NOTFOUND)
			break;
		if (err) print_dba_error();
		fail_unless(err == DBA_OK);
	}

	CHECKED(dba_aof_decoder_start(decoder, "test-aof-04"));
	while (1)
	{
		/* Read all the records */
		dba_err err = dba_aof_decoder_next(decoder, rec);
		if (err == DBA_ERROR && dba_error_get_code() == DBA_ERR_NOTFOUND)
			break;
		if (err) print_dba_error();
		fail_unless(err == DBA_OK);
	}


	dba_record_delete(rec);
#endif

}

/* vim:set ts=4 sw=4: */
