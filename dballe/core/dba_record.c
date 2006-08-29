/*
 * DB-ALLe - Archive for punctual meteorological data
 *
 * Copyright (C) 2005,2006  ARPA-SIM <urpsim@smr.arpa.emr.it>
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

#include "dba_record.h"
#include "dba_var.h"

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>

/* Prototype of dba_record_keyword defined in the separate file generated by
 * gperf */
dba_varinfo dba_record_keyword(const char* tag, int* index);
dba_varinfo dba_record_keyword_byindex(int index);

/*
 * Size of the keyword table.  It should be the number of items in
 * dba_record_keyword.gperf, plus 1
 */
#define KEYWORD_TABLE_SIZE DBA_KEY_COUNT

#define assert_is_dba_record(rec) do { \
		assert((rec) != NULL); \
	} while (0)


/* Structure that holds the value of an item in the hashtable */
typedef struct _dba_item
{
	dba_var var;
	struct _dba_item* next;
}* dba_item;

struct _dba_record
{
	/* The storage for the core keyword data */
	dba_var keydata[KEYWORD_TABLE_SIZE];

	/* The hash table */
	dba_item vars;
};

/* Hash table lists handling */

static void dba_item_delete(dba_item i)
{
	if (i->var != NULL)
		dba_var_delete(i->var);
	free(i);
}

static int dba_record_has_item(dba_record rec, dba_varcode code)
{
	dba_item cur;
	for (cur = rec->vars; cur != NULL; cur = cur->next)
		if (dba_var_code(cur->var) == code)
			return 1;
	return 0;
}

static dba_err dba_record_get_item(dba_record rec, dba_varcode code, dba_item* res)
{
	dba_item cur;
	for (cur = rec->vars; cur != NULL; cur = cur->next)
		if (dba_var_code(cur->var) == code)
		{
			*res = cur;
			return dba_error_ok();
		}

	return dba_error_notfound("looking for parameter \"B%02d%03d\"",
			DBA_VAR_X(code), DBA_VAR_Y(code));
}

static dba_err dba_record_obtain_item(dba_record rec, dba_varcode code, dba_item* res)
{
	dba_item cur;
	assert_is_dba_record(rec);

	for (cur = rec->vars; cur != NULL; cur = cur->next)
		if (dba_var_code(cur->var) == code)
		{
			/* If the item exists, return it */
			*res = cur;
			return dba_error_ok();
		}

	/* Else, create it and prepend it to the chain */
	*res = (dba_item)calloc(1, sizeof(struct _dba_item));
	if (*res == NULL)
		return dba_error_alloc("creating new dba_item");

	(*res)->next = rec->vars;
	rec->vars = *res;

	return dba_error_ok();
}
static void dba_record_rollback_obtain(dba_record rec)
{
	dba_item tmp = rec->vars;
	rec->vars = rec->vars->next;
	dba_item_delete(tmp);
}

static void dba_record_remove_item(dba_record rec, dba_varcode code)
{
	dba_item cur;
	if (rec->vars == NULL)
		return;
	if (dba_var_code(rec->vars->var) == code)
	{
		dba_item next = rec->vars->next;
		dba_item_delete(rec->vars);
		rec->vars = next;
		return;
	}
	for (cur = rec->vars; cur->next != NULL; cur = cur->next)
		if (dba_var_code(cur->next->var) == code)
		{
			dba_item next = cur->next->next;
			dba_item_delete(cur->next);
			cur->next = next;
			return;
		}
}

static void dba_record_remove_dba_item(dba_record rec, dba_item i)
{
	dba_item cur;
	if (rec->vars == NULL)
		return;
	if (rec->vars == i)
	{
		dba_item next = rec->vars->next;
		dba_item_delete(rec->vars);
		rec->vars = next;
		return;
	}
	for (cur = rec->vars; cur->next != NULL; cur = cur->next)
		if (rec->vars == i)
		{
			dba_item next = cur->next->next;
			dba_item_delete(cur->next);
			cur->next = next;
			return;
		}
}

dba_err dba_record_keyword_info(dba_keyword keyword, dba_varinfo* info)
{
	if ((*info = dba_record_keyword_byindex(keyword)) == NULL)
		return dba_error_notfound("looking for informations about keyword #%d", keyword);
	return dba_error_ok();
}

/* Constructor and destructor */

dba_err dba_record_create(dba_record* rec)
{
	*rec = (dba_record)calloc(1, sizeof(struct _dba_record));
	if (*rec == NULL)
		return dba_error_alloc("creating new dba_record");

	assert_is_dba_record(*rec);

	return dba_error_ok();
}

void dba_record_delete(dba_record rec)
{
	assert_is_dba_record(rec);
	dba_record_clear(rec);
	free(rec);
}

void dba_record_clear_vars(dba_record rec)
{
	assert_is_dba_record(rec);
	while (rec->vars != NULL)
	{
		dba_item next = rec->vars->next;
		dba_item_delete(rec->vars);
		rec->vars = next;
	}
}

void dba_record_clear(dba_record rec)
{
	int i;
	assert_is_dba_record(rec);
	for (i = 0; i < KEYWORD_TABLE_SIZE; i++)
		if (rec->keydata[i] != NULL)
		{
			dba_var_delete(rec->keydata[i]);
			rec->keydata[i] = NULL;
		}
	dba_record_clear_vars(rec);
}

dba_err dba_record_copy(dba_record dest, dba_record source)
{
	int i;
	assert_is_dba_record(dest);
	assert_is_dba_record(source);

	/* Prevent self-copying */
	if (dest == source)
		return dba_error_ok();

	/* Copy the keyword table first */
	for (i = 0; i < KEYWORD_TABLE_SIZE; i++)
	{
		if (dest->keydata[i] != NULL)
		{
			free(dest->keydata[i]);
			dest->keydata[i] = NULL;
		}
		if (source->keydata[i] != NULL)
			DBA_RUN_OR_RETURN(dba_var_copy(source->keydata[i], &(dest->keydata[i])));
	}

	/* Then the variable list */
	if (dest->vars)
		dba_record_clear_vars(dest);

	if (source->vars)
	{
		dba_item* t = &(dest->vars);
		dba_item c;
		for (c = source->vars; c != NULL; c = c->next)
		{
			(*t) = (dba_item)calloc(1, sizeof(struct _dba_item));
			if ((*t) == NULL)
				return dba_error_alloc("creating new dba_item");

			DBA_RUN_OR_RETURN(dba_var_copy(c->var, &((*t)->var)));
			t = &((*t)->next);
		}
	}
	return dba_error_ok();
}

dba_err dba_record_add(dba_record dest, dba_record source)
{
	int i;
	assert_is_dba_record(dest);
	assert_is_dba_record(source);

	/* Copy the keyword table first */
	for (i = 0; i < KEYWORD_TABLE_SIZE; i++)
		if (source->keydata[i] != NULL)
		{
			if (dest->keydata[i] != NULL)
				free(dest->keydata[i]);
			DBA_RUN_OR_RETURN(dba_var_copy(source->keydata[i], &(dest->keydata[i])));
		}

	/* Then the variables list */
	if (source->vars)
	{
		dba_item* t = &(dest->vars);
		dba_item c;
		for ( ; (*t) != NULL; t = &((*t)->next))
			;
		for (c = source->vars; c != NULL; c = c->next)
		{
			(*t) = (dba_item)calloc(1, sizeof(struct _dba_item));
			if ((*t) == NULL)
				return dba_error_alloc("creating new dba_item");

			DBA_RUN_OR_RETURN(dba_var_copy(c->var, &((*t)->var)));

			t = &((*t)->next);
		}
	}
	return dba_error_ok();
}

dba_err dba_record_difference(dba_record dest, dba_record source1, dba_record source2)
{
	int i;
	assert_is_dba_record(dest);
	assert_is_dba_record(source1);
	assert_is_dba_record(source2);

	/* Copy the keyword table first */
	for (i = 0; i < KEYWORD_TABLE_SIZE; i++)
	{
		if (dest->keydata[i] != NULL)
		{
			free(dest->keydata[i]);
			dest->keydata[i] = NULL;
		}
		if (source2->keydata[i] != NULL && 
				(source1->keydata[i] == NULL ||
				 strcmp(dba_var_value(source1->keydata[i]), dba_var_value(source2->keydata[i])) == 1))
		{
			DBA_RUN_OR_RETURN(dba_var_copy(source2->keydata[i], &(dest->keydata[i])));
		}
	}

	/* Then the variables list */
	if (dest->vars)
		dba_record_clear_vars(dest);

	if (source2->vars)
	{
		dba_item c;
		for (c = source2->vars; c != NULL; c = c->next)
		{
			dba_varcode varcode = dba_var_code(c->var);
			dba_item s1item;

			if (dba_record_get_item(source1, varcode, &s1item) == DBA_ERROR ||
					strcmp(dba_var_value(s1item->var), dba_var_value(c->var)) != 0)
			{
				dba_item i;
				DBA_RUN_OR_RETURN(dba_record_obtain_item(dest, varcode, &i));
				
				/* Set the value */
				if (i->var == NULL)
					DBA_RUN_OR_RETURN(dba_var_copy(c->var, &(i->var)));
				else
					DBA_RUN_OR_RETURN(dba_var_setc(i->var, dba_var_value(c->var)));
			}
		}
	}
	return dba_error_ok();
}

static dba_err get_key(dba_record rec, dba_keyword parameter, dba_var* var)
{
	assert_is_dba_record(rec);

	if (parameter < 0 || parameter >= DBA_KEY_COUNT)
		return dba_error_notfound("keyword #%d is not in the range of valid keywords", parameter);

	/* Lookup the variable in the keyword table */
	if (rec->keydata[parameter] == NULL)
	{
		dba_varinfo info;
		DBA_RUN_OR_RETURN(dba_record_keyword_info(parameter, &info));
		return dba_error_notfound("looking for parameter %s", info->desc);
	}

	*var = rec->keydata[parameter];

	return dba_error_ok();
}

static dba_err get_var(dba_record rec, dba_varcode code, dba_var* var)
{
	dba_item i;

	assert_is_dba_record(rec);

	/* Lookup the variable in the hash table */
	DBA_RUN_OR_RETURN(dba_record_get_item(rec, code, &i));

	*var = i->var, var;

	return dba_error_ok();
}

dba_var dba_record_key_peek(dba_record rec, dba_keyword parameter)
{
	assert_is_dba_record(rec);

	if (parameter < 0 || parameter >= DBA_KEY_COUNT)
		return NULL;
	return rec->keydata[parameter];
}

dba_var dba_record_var_peek(dba_record rec, dba_varcode code)
{
	assert_is_dba_record(rec);

	dba_item i;
	for (i = rec->vars; i != NULL; i = i->next)
		if (dba_var_code(i->var) == code)
			return i->var;
	return NULL;
}

const char* dba_record_key_peek_value(dba_record rec, dba_keyword parameter)
{
	dba_var var = dba_record_key_peek(rec, parameter);
	return var != NULL ? dba_var_value(var) : NULL;
}

const char* dba_record_var_peek_value(dba_record rec, dba_varcode code)
{
	dba_var var = dba_record_var_peek(rec, code);
	return var != NULL ? dba_var_value(var) : NULL;
}

dba_err dba_record_contains_key(dba_record rec, dba_keyword parameter, int *found)
{
	assert_is_dba_record(rec);

	if (parameter < 0 || parameter >= DBA_KEY_COUNT)
		return dba_error_notfound("looking for informations about parameter #%d", parameter);

	*found = (rec->keydata[parameter] != NULL);

	return dba_error_ok();
}

dba_err dba_record_contains_var(dba_record rec, dba_varcode code, int *found)
{
	assert_is_dba_record(rec);

	*found = dba_record_has_item(rec, code);

	return dba_error_ok();
}

dba_err dba_record_key_enq(dba_record rec, dba_keyword parameter, dba_var* var)
{
	dba_var myvar;
	DBA_RUN_OR_RETURN(get_key(rec, parameter, &myvar));

	/* Return a reference to the variable */
	return dba_var_copy(myvar, var);
}

dba_err dba_record_var_enq(dba_record rec, dba_varcode code, dba_var* var)
{
	dba_var myvar;
	DBA_RUN_OR_RETURN(get_var(rec, code, &myvar));

	/* Return a reference to the variable */
	return dba_var_copy(myvar, var);
}

dba_err dba_record_key_enqi(dba_record rec, dba_keyword parameter, int* val)
{
	dba_var var;
	DBA_RUN_OR_RETURN(get_key(rec, parameter, &var));
	return dba_var_enqi(var, val);
}

dba_err dba_record_var_enqi(dba_record rec, dba_varcode code, int* val)
{
	dba_var var;
	DBA_RUN_OR_RETURN(get_var(rec, code, &var));
	return dba_var_enqi(var, val);
}

dba_err dba_record_key_enqd(dba_record rec, dba_keyword parameter, double* val)
{
	dba_var var;
	DBA_RUN_OR_RETURN(get_key(rec, parameter, &var));
	return dba_var_enqd(var, val);
}

dba_err dba_record_var_enqd(dba_record rec, dba_varcode code, double* val)
{
	dba_var var;
	DBA_RUN_OR_RETURN(get_var(rec, code, &var));
	return dba_var_enqd(var, val);
}

dba_err dba_record_key_enqc(dba_record rec, dba_keyword parameter, const char** val)
{
	dba_var var;
	DBA_RUN_OR_RETURN(get_key(rec, parameter, &var));
	return dba_var_enqc(var, val);
}

dba_err dba_record_var_enqc(dba_record rec, dba_varcode code, const char** val)
{
	dba_var var;
	DBA_RUN_OR_RETURN(get_var(rec, code, &var));
	return dba_var_enqc(var, val);
}

dba_err dba_record_key_set(dba_record rec, dba_keyword parameter, dba_var var)
{
	dba_varinfo info;
	assert_is_dba_record(rec);

	if (parameter < 0 || parameter >= DBA_KEY_COUNT)
		return dba_error_notfound("keyword #%d is not in the range of valid keywords", parameter);
	if (rec->keydata[parameter] != NULL)
	{
		dba_var_delete(rec->keydata[parameter]);
		rec->keydata[parameter] = NULL;
	}
	info = dba_record_keyword_byindex(parameter);
	DBA_RUN_OR_RETURN(dba_var_convert(var, info, &(rec->keydata[parameter])));
	return dba_error_ok();
}

dba_err dba_record_var_set(dba_record rec, dba_varcode code, dba_var var)
{
	assert_is_dba_record(rec);

	dba_err err = DBA_OK;
	dba_item i = NULL;

	/* If var is undef, remove this variable from the record */
	if (dba_var_value(var) == NULL)
		dba_record_remove_item(rec, code);
	else
	{
		/* Lookup the variable in the hash table */
		DBA_RUN_OR_GOTO(fail1, dba_record_obtain_item(rec, code, &i));

		/* Set the value of the variable */
		if (i->var != NULL)
		{
			dba_var_delete(i->var);
			i->var = NULL;
		}
		if (dba_var_code(var) == code)
			DBA_RUN_OR_GOTO(fail1, dba_var_copy(var, &(i->var)));
		else
		{
			dba_varinfo info;
			DBA_RUN_OR_GOTO(fail1, dba_varinfo_query_local(code, &info));
			DBA_RUN_OR_GOTO(fail1, dba_var_convert(var, info, &(i->var)));
		}
	}
	return dba_error_ok();
fail1:
	if (i != NULL)
		dba_record_remove_dba_item(rec, i);
	return err;
}

dba_err dba_record_var_set_direct(dba_record rec, dba_var var)
{
	dba_item i;
	dba_varcode varcode = dba_var_code(var);

	assert_is_dba_record(rec);

	/* If var is undef, remove this variable from the record */
	if (dba_var_value(var) == NULL)
	{
		dba_record_remove_item(rec, varcode);
	} else {
		/* Lookup the variable in the hash table */
		DBA_RUN_OR_RETURN(dba_record_obtain_item(rec, varcode, &i));

		/* Set the value of the variable */
		if (i->var != NULL)
		{
			dba_var_delete(i->var);
			i->var = NULL;
		}
		DBA_RUN_OR_RETURN(dba_var_copy(var, &(i->var)));
	}
	return dba_error_ok();
}

dba_err dba_record_set_ana_context(dba_record rec)
{
	DBA_RUN_OR_RETURN(dba_record_key_seti(rec, DBA_KEY_YEAR, 1000));
	DBA_RUN_OR_RETURN(dba_record_key_seti(rec, DBA_KEY_MONTH, 1));
	DBA_RUN_OR_RETURN(dba_record_key_seti(rec, DBA_KEY_DAY, 1));
	DBA_RUN_OR_RETURN(dba_record_key_seti(rec, DBA_KEY_HOUR, 0));
	DBA_RUN_OR_RETURN(dba_record_key_seti(rec, DBA_KEY_MIN, 0));
	DBA_RUN_OR_RETURN(dba_record_key_seti(rec, DBA_KEY_SEC, 0));
	DBA_RUN_OR_RETURN(dba_record_key_seti(rec, DBA_KEY_LEVELTYPE, 257));
	DBA_RUN_OR_RETURN(dba_record_key_seti(rec, DBA_KEY_L1, 0));
	DBA_RUN_OR_RETURN(dba_record_key_seti(rec, DBA_KEY_L2, 0));
	DBA_RUN_OR_RETURN(dba_record_key_seti(rec, DBA_KEY_PINDICATOR, 0));
	DBA_RUN_OR_RETURN(dba_record_key_seti(rec, DBA_KEY_P1, 0));
	DBA_RUN_OR_RETURN(dba_record_key_seti(rec, DBA_KEY_P2, 0));
	DBA_RUN_OR_RETURN(dba_record_key_seti(rec, DBA_KEY_REP_COD, 254));
	return dba_error_ok();
}

dba_err dba_record_key_seti(dba_record rec, dba_keyword parameter, int value)
{
	assert_is_dba_record(rec);

	if (parameter < 0 || parameter >= DBA_KEY_COUNT)
		return dba_error_notfound("keyword #%d is not in the range of valid keywords", parameter);

	if (rec->keydata[parameter] != NULL)
		return dba_var_seti(rec->keydata[parameter], value);
	else
		return dba_var_createi(dba_record_keyword_byindex(parameter), value, &(rec->keydata[parameter]));
}

dba_err dba_record_var_seti(dba_record rec, dba_varcode code, int value)
{
	dba_err err;
	dba_item i = NULL;

	assert_is_dba_record(rec);

	/* Lookup the variable in the hash table */
	DBA_RUN_OR_RETURN(dba_record_obtain_item(rec, code, &i));

	/* Set the integer value of the variable */
	if (i->var == NULL)
	{
		dba_varinfo info;
		DBA_RUN_OR_GOTO(fail, dba_varinfo_query_local(code, &info));
		DBA_RUN_OR_GOTO(fail, dba_var_createi(info, value, &(i->var)));
	}
	else
		DBA_RUN_OR_GOTO(fail, dba_var_seti(i->var, value));

	return dba_error_ok();

fail:
	if (i != NULL)
		dba_record_rollback_obtain(rec);
	return err;
}

dba_err dba_record_key_setd(dba_record rec, dba_keyword parameter, double value)
{
	assert_is_dba_record(rec);

	if (parameter < 0 || parameter >= DBA_KEY_COUNT)
		return dba_error_notfound("keyword #%d is not in the range of valid keywords", parameter);

	if (rec->keydata[parameter] != NULL)
		return dba_var_setd(rec->keydata[parameter], value);
	else
		return dba_var_created(dba_record_keyword_byindex(parameter), value, &(rec->keydata[parameter]));
}

dba_err dba_record_var_setd(dba_record rec, dba_varcode code, double value)
{
	dba_err err;
	dba_item i = NULL;

	assert_is_dba_record(rec);

	/* Lookup the variable in the hash table */
	DBA_RUN_OR_RETURN(dba_record_obtain_item(rec, code, &i));

	/* Set the double value of the variable */
	if (i->var == NULL)
	{
		dba_varinfo info;
		DBA_RUN_OR_GOTO(fail, dba_varinfo_query_local(code, &info));
		DBA_RUN_OR_GOTO(fail, dba_var_created(info, value, &(i->var)));
	}
	else
		DBA_RUN_OR_GOTO(fail, dba_var_setd(i->var, value));

	return dba_error_ok();

fail:
	if (i != NULL)
		dba_record_rollback_obtain(rec);
	return err;
}

dba_err dba_record_key_setc(dba_record rec, dba_keyword parameter, const char* value)
{
	assert_is_dba_record(rec);

	if (parameter < 0 || parameter >= DBA_KEY_COUNT)
		return dba_error_notfound("keyword #%d is not in the range of valid keywords", parameter);

	if (rec->keydata[parameter] != NULL)
		return dba_var_setc(rec->keydata[parameter], value);
	else
		return dba_var_createc(dba_record_keyword_byindex(parameter), value, &(rec->keydata[parameter]));
}

dba_err dba_record_var_setc(dba_record rec, dba_varcode code, const char* value)
{
	dba_err err;
	dba_item i = NULL;

	assert_is_dba_record(rec);

	/* Lookup the variable in the hash table */
	DBA_RUN_OR_RETURN(dba_record_obtain_item(rec, code, &i));

	/* Set the string value of the variable */
	if (i->var == NULL)
	{
		dba_varinfo info;
		DBA_RUN_OR_GOTO(fail, dba_varinfo_query_local(code, &info));
		DBA_RUN_OR_GOTO(fail, dba_var_createc(info, value, &(i->var)));
	}
	else
		DBA_RUN_OR_GOTO(fail, dba_var_setc(i->var, value));
	return dba_error_ok();

fail:
	if (i != NULL)
		dba_record_rollback_obtain(rec);
	return err;
}

dba_err dba_record_key_unset(dba_record rec, dba_keyword parameter)
{
	assert_is_dba_record(rec);

	if (parameter < 0 || parameter >= DBA_KEY_COUNT)
		return dba_error_notfound("keyword #%d is not in the range of valid keywords", parameter);

	/* Delete old value if it exists */
	if (rec->keydata[parameter] != NULL)
	{
		dba_var_delete(rec->keydata[parameter]);
		rec->keydata[parameter] = NULL;
	}
	return dba_error_ok();
}

dba_err dba_record_var_unset(dba_record rec, dba_varcode code)
{
	assert_is_dba_record(rec);

	dba_record_remove_item(rec, code);

	return dba_error_ok();
}

void dba_record_print(dba_record rec, FILE* out)
{
	int i;
	dba_record_cursor cur;
	for (i = 0; i < KEYWORD_TABLE_SIZE; i++)
		if (rec->keydata[i] != NULL)
			dba_var_print(rec->keydata[i], out);

	for (cur = dba_record_iterate_first(rec); cur != NULL; cur = dba_record_iterate_next(rec, cur))
		dba_var_print(dba_record_cursor_variable(cur), out);
}

int dba_record_equals(dba_record rec1, dba_record rec2)
{
	int i;
	dba_record_cursor cur;

	/* First compare the keywords */
	for (i = 0; i < KEYWORD_TABLE_SIZE; i++)
		if (rec1->keydata[i] == NULL && rec2->keydata[i] == NULL)
			continue;
		else
			if (!dba_var_equals(rec1->keydata[i], rec2->keydata[i]))
				return 0;

	/* Then compare the hash tables */
	for (cur = dba_record_iterate_first(rec1); cur != NULL; cur = dba_record_iterate_next(rec1, cur))
	{
		dba_item item2;
		dba_varcode code = dba_var_code(cur->var);
		if (!dba_record_has_item(rec2, code))
			return 0;

		for (item2 = rec2->vars; item2 != NULL; item2 = item2->next)
			if (dba_var_code(item2->var) == code)
			{
				if (!dba_var_equals(cur->var, item2->var))
					return 0;
				break;
			}
	}

	/* Check for the items in the second one not present in the first one */
	for (cur = dba_record_iterate_first(rec2); cur != NULL; cur = dba_record_iterate_next(rec2, cur))
	{
		dba_varcode code = dba_var_code(cur->var);
		if (!dba_record_has_item(rec2, code))
			return 0;
	}

	return 1;
}

void dba_record_diff(dba_record rec1, dba_record rec2, int* diffs, FILE* out)
{
	int i;
	dba_record_cursor cur;

	/* First compare the keywords */
	for (i = 0; i < KEYWORD_TABLE_SIZE; i++)
	{
		if (rec1->keydata[i] == NULL && rec2->keydata[i] == NULL)
			continue;
		else
			dba_var_diff(rec1->keydata[i], rec2->keydata[i], diffs, out);
	}

	/* Then compare the hash tables */
	for (cur = dba_record_iterate_first(rec1); cur != NULL; cur = dba_record_iterate_next(rec1, cur))
	{
		dba_varcode code = dba_var_code(cur->var);
		if (!dba_record_has_item(rec2, code))
		{
			fprintf(out, "Variable %d%02d%03d only exists in first record\n",
					DBA_VAR_F(code), DBA_VAR_X(code), DBA_VAR_Y(code));
			(*diffs)++;
		}
		{
			dba_item item2;
			for (item2 = rec2->vars; item2 != NULL; item2 = item2->next)
				if (dba_var_code(item2->var) == code)
				{
					dba_var_diff(cur->var, item2->var, diffs, out);
					break;
				}
		}
	}

	/* Check for the items in the second one not present in the first one */
	for (cur = dba_record_iterate_first(rec2); cur != NULL; cur = dba_record_iterate_next(rec2, cur))
	{
		dba_varcode code = dba_var_code(cur->var);
		if (!dba_record_has_item(rec2, code))
		{
			fprintf(out, "Variable %d%02d%03d only exists in second record\n",
					DBA_VAR_F(code), DBA_VAR_X(code), DBA_VAR_Y(code));
			(*diffs)++;
		}
	}
}

dba_record_cursor dba_record_iterate_first(dba_record rec)
{
	assert_is_dba_record(rec);

	if (rec->vars != NULL)
		return rec->vars;

	return NULL;
}

dba_record_cursor dba_record_iterate_next(dba_record rec, dba_record_cursor cur)
{
	assert_is_dba_record(rec);

	/* Try first to see if there's something else in the chain */
	if (cur->next != NULL)
		return cur->next;

	return NULL;
}

dba_var dba_record_cursor_variable(dba_record_cursor cur)
{
	return cur->var;
}


static inline int peek_int(dba_record rec, dba_keyword key)
{
	const char* s = dba_record_key_peek_value(rec, key);
	return s != NULL ? strtol(s, 0, 10) : -1;
}

static inline int min_with_undef(int v1, int v2)
{
	if (v1 == -1)
		return v2;
	if (v2 == -1)
		return v1;
	return v1 < v2 ? v1 : v2;
}

static inline int max_with_undef(int v1, int v2)
{
	if (v1 == -1)
		return v2;
	if (v2 == -1)
		return v1;
	return v1 > v2 ? v1 : v2;
}

static inline int max_days(int y, int m)
{
	int days[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
	if (m != 2)
		return days[m-1];
	else
		return (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0)) ? 29 : 28;
}

/* Buf must be at least 25 bytes long; values must be at least 6 ints long */
dba_err dba_record_parse_date_extremes(dba_record rec, int* minvalues, int* maxvalues)
{
	dba_keyword names[] = { DBA_KEY_YEAR, DBA_KEY_MONTH, DBA_KEY_DAY, DBA_KEY_HOUR, DBA_KEY_MIN, DBA_KEY_SEC };
	dba_keyword min_names[] = { DBA_KEY_YEARMIN, DBA_KEY_MONTHMIN, DBA_KEY_DAYMIN, DBA_KEY_HOURMIN, DBA_KEY_MINUMIN, DBA_KEY_SECMIN };
	dba_keyword max_names[] = { DBA_KEY_YEARMAX, DBA_KEY_MONTHMAX, DBA_KEY_DAYMAX, DBA_KEY_HOURMAX, DBA_KEY_MINUMAX, DBA_KEY_SECMAX };
	int i;

	/* Get the year */

	for (i = 0; i < 6; i++)
	{
		int val = peek_int(rec, names[i]);
		int min = peek_int(rec, min_names[i]);
		int max = peek_int(rec, max_names[i]);

		minvalues[i] = max_with_undef(val, min);
		maxvalues[i] = min_with_undef(val, max);

		if (i > 0 &&
				((minvalues[i-1] == -1 && minvalues[i] != -1) ||
				 (maxvalues[i-1] == -1 && maxvalues[i] != -1)))
		{
			dba_varinfo key1 = dba_record_keyword_byindex(names[i - 1]);
			dba_varinfo key2 = dba_record_keyword_byindex(names[i]);

			return dba_error_consistency("%s extremes are unset but %s extremes are set",
					key1->desc, key2->desc);
		}
	}

	/* Now values is either 6 times -1, 6 values, or X values followed by 6-X times -1 */

	/* If one of the extremes has been selected, fill in the blanks */

	if (minvalues[0] != -1)
	{
		minvalues[1] = minvalues[1] != -1 ? minvalues[1] : 1;
		minvalues[2] = minvalues[2] != -1 ? minvalues[2] : 1;
		minvalues[3] = minvalues[3] != -1 ? minvalues[3] : 0;
		minvalues[4] = minvalues[4] != -1 ? minvalues[4] : 0;
		minvalues[5] = minvalues[5] != -1 ? minvalues[5] : 0;
	}

	if (maxvalues[0] != -1)
	{
		maxvalues[1] = maxvalues[1] != -1 ? maxvalues[1] : 12;
		maxvalues[2] = maxvalues[2] != -1 ? maxvalues[2] : max_days(maxvalues[0], maxvalues[1]);
		maxvalues[3] = maxvalues[3] != -1 ? maxvalues[3] : 23;
		maxvalues[4] = maxvalues[4] != -1 ? maxvalues[4] : 59;
		maxvalues[5] = maxvalues[5] != -1 ? maxvalues[5] : 59;
	}

	return dba_error_ok();
}

/* vim:set ts=4 sw=4: */
