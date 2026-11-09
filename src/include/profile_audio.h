// ----------------------------------------------------------------------------
// profile_audio.h
//
// Audio tab of the profile edit window (see MODERNIZATION.md): PortAudio
// input (RX) and output (TX) device, like JTDX. Sample rates, corrections and
// other audio systems (OSS, PulseAudio) stay in Configure.
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

#ifndef PROFILE_AUDIO_H
#define PROFILE_AUDIO_H

class ProfileStore;
struct AudioTab;

// Create the Audio tab group at x, y, w, h inside the current Fl_Tabs and fill
// it from the profile.
AudioTab* audio_tab_create(int x, int y, int w, int h, ProfileStore& store);

// Write the Audio tab values to the profile.
bool audio_tab_save(const AudioTab* tab, ProfileStore& store);

void audio_tab_delete(AudioTab* tab);

#endif // PROFILE_AUDIO_H
