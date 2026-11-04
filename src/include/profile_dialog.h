// ----------------------------------------------------------------------------
// profile_dialog.h
//
// Profile selection window shown when fldigi starts (see MODERNIZATION.md).
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

#ifndef PROFILE_DIALOG_H
#define PROFILE_DIALOG_H

#include <string>

// Show the profile selection window and wait until the user opens a profile
// or cancels. root is the profiles folder (HomeDir + "profiles/").
// Returns the base callsign of the chosen profile, or an empty string when
// the user cancels.
std::string profile_select_dialog(const std::string& root);

#endif // PROFILE_DIALOG_H
