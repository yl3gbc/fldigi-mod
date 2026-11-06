// ----------------------------------------------------------------------------
// profile_radio.h
//
// Radio tab of the profile edit window (see MODERNIZATION.md): rig control
// (none, flrig or a Hamlib model), CAT port and speed, PTT (VOX / CAT / DTR /
// RTS), PTT port, Tx delay, tone before transmit, Test CAT and Test PTT.
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

#ifndef PROFILE_RADIO_H
#define PROFILE_RADIO_H

class ProfileStore;
struct RadioTab;

// Create the Radio tab group at x, y, w, h inside the current Fl_Tabs and fill
// it from the profile. Fields the profile does not have yet show the current
// configuration, the same values the profile gets when it is opened.
RadioTab* radio_tab_create(int x, int y, int w, int h, ProfileStore& store);

// Write the Radio tab values to the profile.
bool radio_tab_save(const RadioTab* tab, ProfileStore& store);

void radio_tab_delete(RadioTab* tab);

#endif // PROFILE_RADIO_H
