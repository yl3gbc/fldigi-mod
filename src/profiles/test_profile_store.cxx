// ----------------------------------------------------------------------------
// test_profile_store.cxx
//
// Test for ProfileStore. Exit status 0 = PASS, 1 = FAIL.
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

#include <cstdio>
#include <filesystem>
#include <map>
#include <string>
#include <system_error>

#include <unistd.h>

#include "profile_store.h"

static int failures = 0;

static void check(bool ok, const char* what)
{
	std::printf("%s: %s\n", ok ? "ok  " : "FAIL", what);
	if (!ok)
		failures++;
}

int main()
{
	std::error_code ec;
	std::filesystem::path tmp = std::filesystem::temp_directory_path(ec);
	if (ec) {
		std::printf("FAIL: no temporary directory\n");
		return 1;
	}
	std::filesystem::path root = tmp / ("fldigi-profile-test-" + std::to_string(::getpid()));
	std::filesystem::remove_all(root, ec);
	std::string r = root.string() + "/";

	check(ProfileStore::normalize_callsign(" yl3gbc ") == "YL3GBC", "normalize to upper case");
	check(ProfileStore::is_base_callsign("YL3GBC"), "YL3GBC is a base callsign");
	check(!ProfileStore::is_base_callsign("YL3GBC/P"), "YL3GBC/P is not a base callsign");
	check(!ProfileStore::is_base_callsign(""), "empty is not a base callsign");
	check(ProfileStore::db_path(r, "yl3gbc") == r + "YL3GBC/yl3gbc.duckdb", "folder upper case, file lower case");

	{
		ProfileStore p;
		check(!p.open(r, "YL3GBC/P"), "refuse to open YL3GBC/P as profile");
		check(p.open(r, "yl3gbc"), "create profile yl3gbc");
		check(p.callsign() == "YL3GBC", "profile callsign is YL3GBC");
		check(std::filesystem::exists(r + "YL3GBC/yl3gbc.duckdb"), "database file created");
		check(p.set_note("Mājas stacija"), "set note");
		check(p.set_setting("MYCALL", "YL3GBC"), "set MYCALL");
		check(p.set_setting("MYQTH", "Lielvārde"), "set MYQTH");
		check(p.set_setting("MYQTH", "Rīga"), "replace MYQTH");
	}

	{
		ProfileStore p;
		check(p.open(r, "YL3GBC"), "reopen profile");
		std::string v;
		bool found = false;
		check(p.get_note(v) && v == "Mājas stacija", "note kept after reopen");
		check(p.get_setting("MYQTH", v, found) && found && v == "Rīga", "replaced value kept");
		check(p.get_setting("NOSUCHTAG", v, found) && !found, "missing tag not found");
		std::map<std::string, std::string> all;
		check(p.get_all_settings(all) && all.size() == 2, "two settings stored");
		check(p.remove_setting("MYCALL") && p.get_all_settings(all) && all.size() == 1, "remove setting");
	}

	std::filesystem::remove_all(root, ec);

	std::printf("%s\n", failures ? "FAIL" : "PASS");
	return failures ? 1 : 0;
}
