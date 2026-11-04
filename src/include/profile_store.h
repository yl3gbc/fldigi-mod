// ----------------------------------------------------------------------------
// profile_store.h
//
// DuckDB storage for one fldigi station profile.
//
// Layout (see MODERNIZATION.md, "Profilu glabāšana"):
//   <root>/<CALL>/<call>.duckdb
// where <root> is HomeDir + "profiles/", <CALL> is the base callsign in
// upper case and <call> the same callsign in lower case.
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

#ifndef PROFILE_STORE_H
#define PROFILE_STORE_H

#include <map>
#include <string>

struct _duckdb_database;
struct _duckdb_connection;

class ProfileStore
{
public:
	ProfileStore();
	~ProfileStore();

	ProfileStore(const ProfileStore&) = delete;
	ProfileStore& operator=(const ProfileStore&) = delete;

	// Trim spaces and convert to upper case: " yl3gbc " -> "YL3GBC".
	static std::string normalize_callsign(const std::string& callsign);

	// A profile can only be a base callsign: letters and digits, no '/'.
	static bool is_base_callsign(const std::string& callsign);

	// <root>/<CALL>/
	static std::string profile_dir(const std::string& root, const std::string& callsign);

	// <root>/<CALL>/<call>.duckdb
	static std::string db_path(const std::string& root, const std::string& callsign);

	// <root>/_arhivs/
	static std::string archive_root(const std::string& root);

	// Move the whole profile folder <root>/<CALL>/ unchanged to
	// <root>/_arhivs/<CALL>/. When that already exists the archived folder
	// gets a UTC time suffix: <CALL>_YYYYMMDD-HHMMSS. The profile must not
	// be open. On success the archived folder is returned in archived.
	static bool archive_profile(const std::string& root, const std::string& callsign,
				    std::string& archived, std::string& error);

	// Open the profile database, creating the profile folder, the database
	// file and the tables when they do not exist yet. The callsign is
	// normalized first and must be a base callsign.
	bool open(const std::string& root, const std::string& callsign);
	void close();
	bool is_open() const;

	const std::string& callsign() const { return callsign_; }
	const std::string& last_error() const { return error_; }

	// Table "profile": exactly one row.
	bool set_note(const std::string& note);
	bool get_note(std::string& note);

	// Table "settings": fldigi configuration tag -> value as text.
	bool set_setting(const std::string& tag, const std::string& value);
	bool get_setting(const std::string& tag, std::string& value, bool& found);
	bool remove_setting(const std::string& tag);
	bool get_all_settings(std::map<std::string, std::string>& settings);

private:
	bool exec(const char* sql);
	bool create_schema();
	bool set_error(const std::string& msg);

	_duckdb_database*   db_;
	_duckdb_connection* con_;
	std::string         callsign_;
	std::string         error_;
};

#endif // PROFILE_STORE_H
