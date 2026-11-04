// ----------------------------------------------------------------------------
// profile_store.cxx
//
// DuckDB storage for one fldigi station profile.
//
// This file is part of fldigi.
//
// fldigi is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.
//
// fldigi is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
// ----------------------------------------------------------------------------

// Use only the non-deprecated DuckDB C API, so a DuckDB upgrade that removes
// deprecated functions cannot break this file.
#define DUCKDB_API_NO_DEPRECATED

#include <duckdb.h>

#include <cctype>
#include <filesystem>
#include <system_error>
#include <vector>

#include "profile_store.h"

namespace {

struct Row {
	std::vector<std::string> cols;
	std::vector<bool>        null;
};

std::string vector_string(duckdb_vector vec, idx_t row, bool& is_null)
{
	uint64_t* validity = duckdb_vector_get_validity(vec);
	if (validity && !duckdb_validity_row_is_valid(validity, row)) {
		is_null = true;
		return std::string();
	}
	is_null = false;
	duckdb_string_t* data = static_cast<duckdb_string_t*>(duckdb_vector_get_data(vec));
	return std::string(duckdb_string_t_data(&data[row]), duckdb_string_t_length(data[row]));
}

} // namespace

ProfileStore::ProfileStore() : db_(nullptr), con_(nullptr)
{
}

ProfileStore::~ProfileStore()
{
	close();
}

std::string ProfileStore::normalize_callsign(const std::string& callsign)
{
	size_t b = 0, e = callsign.size();
	while (b < e && std::isspace(static_cast<unsigned char>(callsign[b]))) b++;
	while (e > b && std::isspace(static_cast<unsigned char>(callsign[e - 1]))) e--;
	std::string out = callsign.substr(b, e - b);
	for (char& c : out)
		c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
	return out;
}

bool ProfileStore::is_base_callsign(const std::string& callsign)
{
	if (callsign.empty())
		return false;
	for (char c : callsign) {
		unsigned char u = static_cast<unsigned char>(c);
		if (!(std::isupper(u) || std::isdigit(u)))
			return false;
	}
	return true;
}

std::string ProfileStore::profile_dir(const std::string& root, const std::string& callsign)
{
	std::string dir(root);
	if (!dir.empty() && dir.back() != '/')
		dir += '/';
	dir += normalize_callsign(callsign);
	dir += '/';
	return dir;
}

std::string ProfileStore::db_path(const std::string& root, const std::string& callsign)
{
	std::string lower = normalize_callsign(callsign);
	for (char& c : lower)
		c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	return profile_dir(root, callsign) + lower + ".duckdb";
}

bool ProfileStore::set_error(const std::string& msg)
{
	error_ = msg;
	return false;
}

bool ProfileStore::is_open() const
{
	return con_ != nullptr;
}

bool ProfileStore::open(const std::string& root, const std::string& callsign)
{
	close();
	error_.clear();

	std::string call = normalize_callsign(callsign);
	if (!is_base_callsign(call))
		return set_error("not a base callsign: " + callsign);

	std::error_code ec;
	std::filesystem::create_directories(profile_dir(root, call), ec);
	if (ec)
		return set_error("cannot create " + profile_dir(root, call) + ": " + ec.message());

	std::string path = db_path(root, call);
	duckdb_database db = nullptr;
	if (duckdb_open(path.c_str(), &db) == DuckDBError)
		return set_error("cannot open " + path);

	duckdb_connection con = nullptr;
	if (duckdb_connect(db, &con) == DuckDBError) {
		duckdb_close(&db);
		return set_error("cannot connect to " + path);
	}

	db_ = db;
	con_ = con;
	callsign_ = call;

	if (!create_schema()) {
		std::string err = error_;
		close();
		return set_error(err);
	}
	return true;
}

void ProfileStore::close()
{
	if (con_) {
		duckdb_connection con = con_;
		duckdb_disconnect(&con);
		con_ = nullptr;
	}
	if (db_) {
		duckdb_database db = db_;
		duckdb_close(&db);
		db_ = nullptr;
	}
	callsign_.clear();
}

bool ProfileStore::exec(const char* sql)
{
	duckdb_result res;
	if (duckdb_query(con_, sql, &res) == DuckDBError) {
		const char* msg = duckdb_result_error(&res);
		std::string err = msg ? msg : "query failed";
		duckdb_destroy_result(&res);
		return set_error(err);
	}
	duckdb_destroy_result(&res);
	return true;
}

// Run a statement with text parameters. When rows is not null the result
// rows are returned as text columns (only VARCHAR columns are supported).
static bool run(duckdb_connection con, const char* sql,
                const std::vector<std::string>& params,
                std::vector<Row>* rows, std::string& error)
{
	duckdb_prepared_statement stmt = nullptr;
	if (duckdb_prepare(con, sql, &stmt) == DuckDBError) {
		const char* msg = duckdb_prepare_error(stmt);
		error = msg ? msg : "prepare failed";
		duckdb_destroy_prepare(&stmt);
		return false;
	}
	for (size_t i = 0; i < params.size(); i++) {
		if (duckdb_bind_varchar(stmt, i + 1, params[i].c_str()) == DuckDBError) {
			error = "bind failed";
			duckdb_destroy_prepare(&stmt);
			return false;
		}
	}
	duckdb_result res;
	if (duckdb_execute_prepared(stmt, &res) == DuckDBError) {
		const char* msg = duckdb_result_error(&res);
		error = msg ? msg : "execute failed";
		duckdb_destroy_result(&res);
		duckdb_destroy_prepare(&stmt);
		return false;
	}
	if (rows) {
		idx_t ncols = duckdb_column_count(&res);
		for (;;) {
			duckdb_data_chunk chunk = duckdb_fetch_chunk(res);
			if (!chunk)
				break;
			idx_t n = duckdb_data_chunk_get_size(chunk);
			for (idx_t r = 0; r < n; r++) {
				Row row;
				for (idx_t c = 0; c < ncols; c++) {
					bool is_null = false;
					row.cols.push_back(vector_string(duckdb_data_chunk_get_vector(chunk, c), r, is_null));
					row.null.push_back(is_null);
				}
				rows->push_back(row);
			}
			duckdb_destroy_data_chunk(&chunk);
		}
	}
	duckdb_destroy_result(&res);
	duckdb_destroy_prepare(&stmt);
	return true;
}

bool ProfileStore::create_schema()
{
	static const char* tables[] = {
		"CREATE TABLE IF NOT EXISTS profile ("
		" callsign VARCHAR NOT NULL,"
		" note VARCHAR,"
		" created_at TIMESTAMP DEFAULT current_timestamp,"
		" updated_at TIMESTAMP)",

		"CREATE TABLE IF NOT EXISTS settings ("
		" tag VARCHAR PRIMARY KEY,"
		" value VARCHAR)",

		"CREATE TABLE IF NOT EXISTS alt_callsigns ("
		" callsign VARCHAR PRIMARY KEY,"
		" kind VARCHAR NOT NULL DEFAULT 'parasts'"
		"  CHECK (kind IN ('parasts', 'sacensību')))",

		"CREATE TABLE IF NOT EXISTS alt_activations ("
		" callsign VARCHAR NOT NULL REFERENCES alt_callsigns(callsign),"
		" date_from DATE NOT NULL,"
		" date_to DATE,"
		" locator VARCHAR,"
		" event VARCHAR NOT NULL,"
		" PRIMARY KEY (callsign, date_from))"
	};
	for (const char* sql : tables)
		if (!exec(sql))
			return false;

	std::vector<Row> rows;
	if (!run(con_, "SELECT callsign FROM profile", {}, &rows, error_))
		return false;
	if (rows.empty())
		return run(con_, "INSERT INTO profile (callsign) VALUES (?)", { callsign_ }, nullptr, error_);
	if (rows.size() != 1 || rows[0].cols[0] != callsign_)
		return set_error("profile table does not belong to " + callsign_);
	return true;
}

bool ProfileStore::set_note(const std::string& note)
{
	if (!is_open())
		return set_error("profile not open");
	return run(con_, "UPDATE profile SET note = ?, updated_at = current_timestamp",
	           { note }, nullptr, error_);
}

bool ProfileStore::get_note(std::string& note)
{
	if (!is_open())
		return set_error("profile not open");
	std::vector<Row> rows;
	if (!run(con_, "SELECT note FROM profile", {}, &rows, error_))
		return false;
	note = (rows.empty() || rows[0].null[0]) ? std::string() : rows[0].cols[0];
	return true;
}

bool ProfileStore::set_setting(const std::string& tag, const std::string& value)
{
	if (!is_open())
		return set_error("profile not open");
	return run(con_, "INSERT OR REPLACE INTO settings (tag, value) VALUES (?, ?)",
	           { tag, value }, nullptr, error_);
}

bool ProfileStore::get_setting(const std::string& tag, std::string& value, bool& found)
{
	if (!is_open())
		return set_error("profile not open");
	std::vector<Row> rows;
	if (!run(con_, "SELECT value FROM settings WHERE tag = ?", { tag }, &rows, error_))
		return false;
	found = !rows.empty();
	value = (found && !rows[0].null[0]) ? rows[0].cols[0] : std::string();
	return true;
}

bool ProfileStore::remove_setting(const std::string& tag)
{
	if (!is_open())
		return set_error("profile not open");
	return run(con_, "DELETE FROM settings WHERE tag = ?", { tag }, nullptr, error_);
}

bool ProfileStore::get_all_settings(std::map<std::string, std::string>& settings)
{
	if (!is_open())
		return set_error("profile not open");
	std::vector<Row> rows;
	if (!run(con_, "SELECT tag, value FROM settings ORDER BY tag", {}, &rows, error_))
		return false;
	settings.clear();
	for (const Row& r : rows)
		settings[r.cols[0]] = r.null[1] ? std::string() : r.cols[1];
	return true;
}
