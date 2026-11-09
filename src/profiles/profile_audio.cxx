// ----------------------------------------------------------------------------
// profile_audio.cxx
//
// Audio tab of the profile edit window (see MODERNIZATION.md).
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

#include <config.h>

#include <algorithm>
#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

#include <FL/Fl.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Group.H>

#if USE_PORTAUDIO
#include <portaudio.h>
#endif

#include "configuration.h"
#include "gettext.h"
#include "profile_audio.h"
#include "profile_store.h"

namespace {

// AUDIOIO values (see soundconf.h)
const int AUDIO_OSS   = 0;
const int AUDIO_PORT  = 1;
const int AUDIO_PULSE = 2;

// Menu item user data that is not a PortAudio device index
const long ITEM_NONE    = -1;   // no sound device found
const long ITEM_OTHER   = -2;   // OSS or PulseAudio set in Configure
const long ITEM_MISSING = -3;   // stored device is not connected now

struct AudioDev {
	int         index;
	int         prio;
	std::string name;
	std::string label;
	bool        input;
	bool        output;
};

struct AudioDevs {
	std::vector<AudioDev> list;
	int def_in;
	int def_out;
};

} // namespace

struct AudioTab {
	Fl_Group*   group;
	Fl_Choice*  dev[2];          // 0 = input (RX), 1 = output (TX)
	std::string stored_name[2];
	std::string missing_label[2];
};

namespace {

std::string setting_or(ProfileStore& store, const char* tag, const std::string& fallback)
{
	std::string v;
	bool found = false;
	if (store.get_setting(tag, v, found) && found)
		return v;
	return fallback;
}

template <typename T>
std::string to_str(const T& v)
{
	std::ostringstream os;
	os << v;
	return os.str();
}

// Escape a label for an FLTK menu.
std::string menu_escape(const std::string& s)
{
	std::string out;
	for (size_t i = 0; i < s.size(); i++) {
		if (s[i] == '/' || s[i] == '\\')
			out += '\\';
		else if (s[i] == '&')
			out += '&';
		else if (s[i] == '_' && out.empty())
			out += ' ';
		out += s[i];
	}
	return out;
}

bool dev_less(const AudioDev& a, const AudioDev& b)
{
	return a.prio < b.prio;
}

// PortAudio devices, ALSA first as in Configure. Read once.
const AudioDevs& audio_devices()
{
	static AudioDevs devs;
	static bool done = false;
	if (done)
		return devs;
	done = true;
	devs.def_in = devs.def_out = -1;
#if USE_PORTAUDIO
	if (Pa_Initialize() != paNoError)
		return devs;
	devs.def_in = Pa_GetDefaultInputDevice();
	devs.def_out = Pa_GetDefaultOutputDevice();
	int n = Pa_GetDeviceCount();
	for (int i = 0; i < n; i++) {
		const PaDeviceInfo* info = Pa_GetDeviceInfo(i);
		if (!info)
			continue;
		const PaHostApiInfo* api = Pa_GetHostApiInfo(info->hostApi);
		AudioDev d;
		d.index = i;
		d.name = info->name;
		d.label = std::string(api ? api->name : "?") + ": " + info->name;
		d.prio = 3;
		if (api && api->type == paALSA) d.prio = 0;
		else if (api && api->type == paJACK) d.prio = 1;
		else if (api && api->type == paOSS) d.prio = 2;
		d.input = info->maxInputChannels > 0;
		d.output = info->maxOutputChannels > 0;
		devs.list.push_back(d);
	}
	std::stable_sort(devs.list.begin(), devs.list.end(), dev_less);
	Pa_Terminate();
#endif
	return devs;
}

long item_data(const Fl_Choice* c)
{
	const Fl_Menu_Item* m = c->mvalue();
	return m ? reinterpret_cast<long>(m->user_data()) : ITEM_NONE;
}

// Menu position of the item with the given user data, or -1.
int find_item(const Fl_Choice* c, long data)
{
	for (int i = 0; i < c->size() - 1; i++) {
		const Fl_Menu_Item& m = c->menu()[i];
		if (m.label() && reinterpret_cast<long>(m.user_data()) == data)
			return i;
	}
	return -1;
}

void select_default(Fl_Choice* c, int dir)
{
	const AudioDevs& devs = audio_devices();
	int pos = find_item(c, dir == 0 ? devs.def_in : devs.def_out);
	if (pos < 0) {
		for (int i = 0; i < c->size() - 1 && pos < 0; i++)
			if (reinterpret_cast<long>(c->menu()[i].user_data()) >= 0)
				pos = i;
	}
	if (pos >= 0)
		c->value(pos);
}

// "Other" means OSS or PulseAudio for both directions: keep the two together.
void cb_device(Fl_Widget* w, void* arg)
{
	AudioTab* t = static_cast<AudioTab*>(arg);
	int self = w == t->dev[0] ? 0 : 1;
	Fl_Choice* other = t->dev[1 - self];
	bool self_other = item_data(t->dev[self]) == ITEM_OTHER;
	bool other_other = item_data(other) == ITEM_OTHER;
	if (self_other && !other_other) {
		int pos = find_item(other, ITEM_OTHER);
		if (pos >= 0)
			other->value(pos);
	}
	else if (!self_other && other_other)
		select_default(other, 1 - self);
}

void fill_choice(AudioTab* t, int dir, bool other, const std::string& name, int index)
{
	Fl_Choice* c = t->dev[dir];
	const AudioDevs& devs = audio_devices();
	if (other)
		c->add(_("Other (set in Configure)"), 0, 0, reinterpret_cast<void*>(ITEM_OTHER));
	int exact = -1, by_name = -1, count = 0;
	for (size_t i = 0; i < devs.list.size(); i++) {
		const AudioDev& d = devs.list[i];
		if ((dir == 0 && !d.input) || (dir == 1 && !d.output))
			continue;
		int pos = c->add(menu_escape(d.label).c_str(), 0, 0,
				 reinterpret_cast<void*>(static_cast<long>(d.index)));
		count++;
		if (d.name == name) {
			if (by_name < 0)
				by_name = pos;
			if (d.index == index)
				exact = pos;
		}
	}
	if (other) {
		c->value(find_item(c, ITEM_OTHER));
		return;
	}
	if (exact >= 0) {
		c->value(exact);
		return;
	}
	if (by_name >= 0) {
		c->value(by_name);
		return;
	}
	if (!name.empty()) {
		// Keep the stored device when it is not connected now.
		t->missing_label[dir] = name + " " + _("(not connected)");
		int pos = c->add(menu_escape(t->missing_label[dir]).c_str(), 0, 0,
				 reinterpret_cast<void*>(ITEM_MISSING));
		c->value(pos);
		return;
	}
	if (count == 0) {
		c->add(_("No sound device found"), 0, 0, reinterpret_cast<void*>(ITEM_NONE));
		c->value(0);
		c->deactivate();
		return;
	}
	select_default(c, dir);
}

} // namespace

// ----------------------------------------------------------------------------

AudioTab* audio_tab_create(int x, int y, int w, int h, ProfileStore& store)
{
	AudioTab* t = new AudioTab;
	const configuration& d = progdefaults;

	int audio_io = atoi(setting_or(store, "AUDIOIO", to_str(d.btnAudioIOis)).c_str());
	bool other = audio_io == AUDIO_OSS || audio_io == AUDIO_PULSE;

	t->group = new Fl_Group(x, y, w, h, _("Audio"));
	t->dev[0] = new Fl_Choice(x + 140, y + 15, 360, 25, _("Input (RX):"));
	t->dev[1] = new Fl_Choice(x + 140, y + 50, 360, 25, _("Output (TX):"));
	t->stored_name[0] = setting_or(store, "PORTINDEVICE", d.PortInDevice);
	t->stored_name[1] = setting_or(store, "PORTOUTDEVICE", d.PortOutDevice);
	fill_choice(t, 0, other, t->stored_name[0],
		    atoi(setting_or(store, "PORTININDEX", to_str(d.PortInIndex)).c_str()));
	fill_choice(t, 1, other, t->stored_name[1],
		    atoi(setting_or(store, "PORTOUTINDEX", to_str(d.PortOutIndex)).c_str()));
	for (int i = 0; i < 2; i++)
		t->dev[i]->callback(cb_device, t);
	t->group->end();
	return t;
}

bool audio_tab_save(const AudioTab* t, ProfileStore& s)
{
	const AudioDevs& devs = audio_devices();
	const char* name_tag[2] = { "PORTINDEVICE", "PORTOUTDEVICE" };
	const char* index_tag[2] = { "PORTININDEX", "PORTOUTINDEX" };
	bool ok = true;
	bool port = false;
	for (int dir = 0; dir < 2; dir++) {
		long data = item_data(t->dev[dir]);
		if (data < 0)
			continue;   // other, missing or none: keep what is stored
		for (size_t i = 0; i < devs.list.size(); i++) {
			if (devs.list[i].index != data)
				continue;
			ok = ok && s.set_setting(name_tag[dir], devs.list[i].name);
			ok = ok && s.set_setting(index_tag[dir], to_str(data));
			port = true;
		}
	}
	if (port)
		ok = ok && s.set_setting("AUDIOIO", to_str(AUDIO_PORT));
	return ok;
}

void audio_tab_delete(AudioTab* t)
{
	delete t;
}
