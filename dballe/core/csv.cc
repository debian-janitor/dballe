/*
 * dballe/csv - CSV utility functions
 *
 * Copyright (C) 2005--2014  ARPA-SIM <urpsim@smr.arpa.emr.it>
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
/*
 * The FormatInt class is Copyright (c) 2012, Victor Zverovich
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <dballe/core/csv.h>
#include <dballe/core/defs.h>
#include "dballe/core/var.h"
#include <wreport/error.h>
#include <cctype>
#include <iostream>
#include <limits>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <stdint.h>

using namespace std;
using namespace wreport;

namespace dballe {

CSVReader::~CSVReader() {}

std::string CSVReader::unescape(const std::string& csvstr)
{
    if (csvstr.empty()) return csvstr;
    if (csvstr[0] != '"') return csvstr;
    if (csvstr.size() == 1) return csvstr;
    string res;
    bool escape = false;
    for (string::const_iterator i = csvstr.begin() + 1; i != csvstr.end(); ++i)
    {
        if (*i == '"')
        {
            if (!escape)
                escape = true;
            else {
                res += *i;
                escape = false;
            }
        } else
            res += *i;
    }
    return res;
}

int CSVReader::as_int(unsigned col) const
{
    if (cols[col].empty())
        error_consistency::throwf("cannot parse a number from column %u, which is empty", col);
    return strtoul(cols[col].c_str(), 0, 10);
}

int CSVReader::as_int_withmissing(unsigned col) const
{
    if (cols[col].empty()) return MISSING_INT;
    return strtoul(cols[col].c_str(), 0, 10);
}

wreport::Varcode CSVReader::as_varcode(unsigned col) const
{
    return resolve_varcode_safe(cols[col]);
}

bool CSVReader::move_to_data(unsigned number_col)
{
    while (true)
    {
        if (number_col < cols.size()
         && (cols[number_col].empty() || isdigit(cols[number_col][0]) || cols[number_col][0] == '-'))
            break;
        if (!next())
            return false;
    }
    return true;
}

bool CSVReader::next()
{
    cols.clear();

    if (!nextline()) return false;

    // Tokenize the input line
    enum State { BEG, COL, QCOL, EQCOL } state = BEG;
    string col;
    for (string::const_iterator i = line.begin(); i != line.end(); ++i)
    {
        switch (state)
        {
            case BEG:
                switch (*i)
                {
                    case '"':
                        state = QCOL;
                        break;
                    case ',':
                    case '\n':
                        state = BEG;
                        cols.push_back(col);
                        break;
                    default:
                        state = COL;
                        col += *i; break;
                }
                break;
            case COL:
                switch (*i)
                {
                    case ',':
                    case '\n':
                        state = BEG;
                        cols.push_back(col);
                        col.clear();
                        break;
                    default:
                        col += *i;
                        break;
                }
                break;
            case QCOL:
                switch (*i)
                {
                    case '\"':
                        state = EQCOL;
                        break;
                    case '\n':
                        state = BEG;
                        cols.push_back(col);
                        col.clear();
                        break;
                    default:
                        col += *i;
                        break;
                }
                break;
            case EQCOL:
                switch (*i)
                {
                    case ',':
                    case '\n':
                        state = BEG;
                        cols.push_back(col);
                        col.clear();
                        break;
                    default:
                        col += *i;
                        break;
                }
                break;
        }
    }
    if (!col.empty())
        cols.push_back(col);

    return true;
}

bool IstreamCSVReader::nextline()
{
    if (in.eof()) return false;
    getline(in, line);
    if (in.bad()) throw error_system("reading line from CSV input");
    if (in.eof() && line.empty()) return false;
    return true;
}

bool csv_read_next(FILE* in, std::vector<std::string>& cols)
{
	char line[2000];
	char* tok;
	char* stringp;

	if (fgets(line, 2000, in) == NULL)
		return false;

	cols.clear();
	for (stringp = line; (tok = strsep(&stringp, ",")) != NULL; )
		cols.push_back(tok);

	return true;
}

void csv_output_quoted_string(ostream& out, const std::string& str)
{
    if (str.find_first_of("\",") != string::npos)
    {
        out << "\"";
        for (string::const_iterator i = str.begin(); i != str.end(); ++i)
        {
            if (*i == '"')
                out << '"';
            out << *i;
        }
        out << "\"";
    } else
        out << str;
}

namespace {

// FormatInt by Victor Zverovich
// See https://github.com/vitaut/format

const char DIGITS[] =
    "0001020304050607080910111213141516171819"
    "2021222324252627282930313233343536373839"
    "4041424344454647484950515253545556575859"
    "6061626364656667686970717273747576777879"
    "8081828384858687888990919293949596979899";

class FormatInt {
 private:
  // Buffer should be large enough to hold all digits (digits10 + 1),
  // a sign and a null character.
  enum {BUFFER_SIZE = std::numeric_limits<uint64_t>::digits10 + 3};
  char buffer_[BUFFER_SIZE];
  char *str_;

  // Formats value in reverse and returns the number of digits.
  char *FormatDecimal(uint64_t value) {
    char *buffer_end = buffer_ + BUFFER_SIZE;
    *--buffer_end = '\0';
    while (value >= 100) {
      // Integer division is slow so do it for a group of two digits instead
      // of for every digit. The idea comes from the talk by Alexandrescu
      // "Three Optimization Tips for C++". See speed-test for a comparison.
      unsigned index = (value % 100) * 2;
      value /= 100;
      *--buffer_end = DIGITS[index + 1];
      *--buffer_end = DIGITS[index];
    }
    if (value < 10) {
      *--buffer_end = static_cast<char>('0' + value);
      return buffer_end;
    }
    unsigned index = static_cast<unsigned>(value * 2);
    *--buffer_end = DIGITS[index + 1];
    *--buffer_end = DIGITS[index];
    return buffer_end;
  }

 public:
  explicit FormatInt(int value) {
    unsigned abs_value = value;
    bool negative = value < 0;
    if (negative)
      abs_value = 0 - value;
    str_ = FormatDecimal(abs_value);
    if (negative)
      *--str_ = '-';
  }
  explicit FormatInt(unsigned value) : str_(FormatDecimal(value)) {}
  explicit FormatInt(uint64_t value) : str_(FormatDecimal(value)) {}

  unsigned size() const { return buffer_ + BUFFER_SIZE - str_ - 1; }
  const char *c_str() const { return str_; }
  std::string str() const { return str_; }
};

}

CSVWriter::~CSVWriter()
{
}

void CSVWriter::add_value_raw(const char* str)
{
    if (!row.empty())
        row += ',';
    row.append(str);
}

void CSVWriter::add_value_raw(const std::string& str)
{
    if (!row.empty())
        row += ',';
    row.append(str);
}

void CSVWriter::add_value(size_t val)
{
    FormatInt fmt(val);
    add_value_raw(fmt.c_str());
}

void CSVWriter::add_value(unsigned val)
{
    FormatInt fmt(val);
    add_value_raw(fmt.c_str());
}

void CSVWriter::add_value(int val)
{
    FormatInt fmt(val);
    add_value_raw(fmt.c_str());
}

void CSVWriter::add_value_withmissing(int val)
{
    if (val == MISSING_INT)
    {
        if (!row.empty())
            row += ',';
    } else {
        add_value(val);
    }
}

void CSVWriter::add_value(wreport::Varcode val)
{
    if (!row.empty())
        row += ',';

    switch (WR_VAR_F(val))
    {
        case 0: row += 'B'; break;
        case 1: row += 'R'; break;
        case 2: row += 'C'; break;
        case 3: row += 'D'; break;
    }

    unsigned index = WR_VAR_X(val) * 2;
    row += DIGITS[index];
    row += DIGITS[index + 1];

    FormatInt fmt(WR_VAR_Y(val));
    for (unsigned i = 0; i < 3-fmt.size(); ++i)
        row += '0';
    row.append(fmt.c_str());
}

void CSVWriter::add_var_value(const wreport::Var& var)
{
    const char* val = var.value();
    if (!val)
    {
        if (!row.empty())
            row += ',';
        return;
    }

    if (var.info()->is_string())
        add_value(val);
    else
        add_value_raw(val);
}

void CSVWriter::add_value(const char* val)
{
    if (!row.empty())
        row += ',';
    if (!*val) return;
    row += '"';
    for ( ; *val; ++val)
    {
        if (*val == '"')
            row += '"';
        row += *val;
    }
    row += '"';
}

void CSVWriter::add_value(const std::string& val)
{
    if (!row.empty())
        row += ',';
    if (val.empty()) return;
    row += '"';
    for (string::const_iterator i = val.begin(); i != val.end(); ++i)
    {
        if (*i == '"')
            row += '"';
        row += *i;
    }
    row += '"';
}

#if 0
void test_dba_csv()
{
	{
		/* Test dba_item handling */

		dba_item chain = NULL, item = NULL;

		CHECKED(dba_item_obtain(&chain, "cippo", &item));

		fail_unless_chain_is(chain, "cippo", chain);
		fail_unless_chain_hasnt(chain, "lippo");

		CHECKED(dba_item_obtain(&chain, "lippo", &item));

		fail_unless(item != chain);
		
		fail_unless_chain_is(chain, "cippo", chain);
		fail_unless_chain_is(chain, "lippo", item);

		dba_item_remove(&chain, "cippo");
		fail_unless(item == chain);
		
		fail_unless_chain_hasnt(chain, "cippo");
		fail_unless_chain_is(chain, "lippo", item);

		dba_item_remove(&chain, "lippo");

		fail_unless(chain == NULL);

		dba_item_delete(chain);
	}

	{
		/* Hash table handling */
		dba_record rec;
		dba_item item = NULL;

		CHECKED(dba_record_create(&rec));
		fail_unless(rec != NULL);

		fail_unless_hasnt(rec, "cippo");
		CHECKED(dba_record_obtain_item(rec, "cippo", &item));
		fail_unless(item != NULL);
		fail_unless_has(rec, "cippo");
		item->value = strdup("cippo");

		fail_unless_hasnt(rec, "lippo");
		CHECKED(dba_record_obtain_item(rec, "lippo", &item));
		fail_unless(item != NULL);
		fail_unless_has(rec, "cippo");
		fail_unless_has(rec, "lippo");
		item->value = strdup("lippo");

		fail_unless_is_it(rec, "cippo");
		fail_unless_is_it(rec, "lippo");

		dba_record_remove_item(rec, "cippo");
		fail_unless_hasnt(rec, "cippo");
		fail_unless_has(rec, "lippo");

		dba_record_remove_item(rec, "cippo");
		fail_unless_hasnt(rec, "cippo");
		fail_unless_has(rec, "lippo");

		dba_record_remove_item(rec, "pippo");
		fail_unless_hasnt(rec, "cippo");
		fail_unless_has(rec, "lippo");

		dba_record_remove_item(rec, "lippo");
		fail_unless_hasnt(rec, "cippo");
		fail_unless_hasnt(rec, "lippo");

		dba_record_remove_item(rec, "pippo");
		fail_unless_hasnt(rec, "cippo");
		fail_unless_hasnt(rec, "lippo");

		dba_record_delete(rec);
	}

	{
		/* Keyword info handling */
		dba_varinfo* info;
		int index;

		fail_unless(dba_record_keyword("cippo", &index) == NULL);
		fail_unless(dba_record_keyword("zzzip", &index) == NULL);

		fail_unless((info = dba_record_keyword("idstaz_select", &index)) != NULL);
		fail_unless(strcmp(info->desc, "Unique station identifier") == 0);

		fail_unless((info = dba_record_keyword("yearmin", &index)) != NULL);
		fail_unless(strcmp(info->desc, "Year or minimum year queried") == 0);

		fail_unless((info = dba_record_keyword("lat", &index)) != NULL);
		fail_unless(strcmp(info->desc, "Latitude") == 0);

		fail_unless((info = dba_record_keyword("lon", &index)) != NULL);
		fail_unless(strcmp(info->desc, "Longitude") == 0);
	}

	{
		/* Record gets and sets */
		dba_err err;
		int ival;
		dba_record rec;

		CHECKED(dba_record_create(&rec));

		err = dba_enqi(rec, "idstaz_select", &ival);
		fail_unless(err == DBA_ERROR);
		fail_unless(dba_error_get_code() == DBA_ERR_TYPE);

		err = dba_enqi(rec, "lat", &ival);
		fail_unless(err == DBA_ERROR);
		fail_unless(dba_error_get_code() == DBA_ERR_NOTFOUND);

		err = dba_seti(rec, "idstaz_select", 1);
		fail_unless(err == DBA_ERROR);
		fail_unless(dba_error_get_code() == DBA_ERR_TYPE);

		CHECKED(dba_seti(rec, "ana_id", -10));
		CHECKED(dba_seti(rec, "lat", 1234567));
		CHECKED(dba_setd(rec, "lon", 76.54321));
		CHECKED(dba_setc(rec, "yearmin", "1976"));
		CHECKED(dba_setc(rec, "B02121", "456"));

		fail_unless_int_is(rec, "ana_id", -10);
		fail_unless_real_is(rec, "ana_id", -10.0);
		fail_unless_int_is(rec, "lon", 7654321);
		/*fail_unless_float_is(rec, "lon", 76.54321);*/
		fail_unless_real_is(rec, "lon", 76.54321);
		fail_unless_char_is(rec, "lon", "7654321");

		fail_unless_int_is(rec, "lat", 1234567);
		/*fail_unless_float_is(rec, "lat", 12.34567);*/
		fail_unless_real_is(rec, "lat", 12.34567);
		fail_unless_char_is(rec, "lat", "1234567");

		fail_unless_int_is(rec, "yearmin", 1976);
		fail_unless_float_is(rec, "yearmin", 1976);
		fail_unless_real_is(rec, "yearmin", 1976);
		fail_unless_char_is(rec, "yearmin", "1976");

		fail_unless_int_is(rec, "B02121", 456);
		/*fail_unless_float_is(rec, "B02121", 45600000000.0)*/;
		fail_unless_real_is(rec, "B02121", 45600000000.0);
		fail_unless_char_is(rec, "B02121", "456");

		CHECKED(dba_unset(rec, "lat"));

		err = dba_enqi(rec, "lat", &ival);
		fail_unless(err == DBA_ERROR);
		fail_unless(dba_error_get_code() == DBA_ERR_NOTFOUND);

		/* fprintf(stderr, "IVAL: %d\n", ival); */
		/* fprintf(stderr, "DVAL: %f\n", fval); */
		/*
		{
			int i = 7654321;
			double f = (double)i / 100000;
			fprintf(stderr, "I: %d, F: %f\n", i, f);
		}
		*/

		/* See if clear clears */
		dba_record_clear(rec);
		
		err = dba_enqi(rec, "lat", &ival);
		fail_unless(err == DBA_ERROR);
		fail_unless(dba_error_get_code() == DBA_ERR_NOTFOUND);

		dba_record_clear(rec);

		err = dba_enqi(rec, "lat", &ival);
		fail_unless(err == DBA_ERROR);
		fail_unless(dba_error_get_code() == DBA_ERR_NOTFOUND);

		dba_record_delete(rec);
	}
}
#endif

} // namespace dballe

/* vim:set ts=4 sw=4: */
