// ----------------------------------------------------------------------------
// profile_dialog.cxx
//
// Profile selection window shown when fldigi starts (see MODERNIZATION.md).
// fldigi does not run without a profile: the user opens an existing profile,
// creates a new one, or cancels and fldigi exits.
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
#include <cctype>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

#include <FL/Fl.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Hold_Browser.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Return_Button.H>
#include <FL/fl_ask.H>

#include "gettext.h"
#include "profile_dialog.h"
#include "profile_store.h"

namespace {

struct ProfileRow {
	std::string callsign;
	std::string note;
	long        qsos;
};

// Number of QSO records in an ADIF file: count of <EOR> tags.
long adif_qso_count(const std::string& path)
{
	std::ifstream f(path.c_str(), std::ios::binary);
	if (!f)
		return 0;
	std::string data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
	for (char& c : data)
		c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	long n = 0;
	for (size_t p = data.find("<eor>"); p != std::string::npos; p = data.find("<eor>", p + 5))
		n++;
	return n;
}

std::string lower(const std::string& s)
{
	std::string out(s);
	for (char& c : out)
		c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	return out;
}

std::vector<ProfileRow> list_profiles(const std::string& root)
{
	std::vector<ProfileRow> rows;
	std::error_code ec;
	std::filesystem::directory_iterator it(root, ec), end;
	for (; !ec && it != end; it.increment(ec)) {
		if (!it->is_directory(ec))
			continue;
		std::string call = it->path().filename().string();
		if (!ProfileStore::is_base_callsign(call))
			continue;
		if (!std::filesystem::exists(ProfileStore::db_path(root, call), ec))
			continue;
		ProfileRow row;
		row.callsign = call;
		row.qsos = adif_qso_count(ProfileStore::profile_dir(root, call) + lower(call) + ".adif");
		ProfileStore store;
		if (store.open(root, call))
			store.get_note(row.note);
		rows.push_back(row);
	}
	std::sort(rows.begin(), rows.end(),
		  [](const ProfileRow& a, const ProfileRow& b) { return a.callsign < b.callsign; });
	return rows;
}

// ----------------------------------------------------------------------------
// New profile window

struct NewProfile {
	Fl_Double_Window* win;
	Fl_Input*         call;
	Fl_Input*         note;
	std::string       root;
	std::string       created;
};

void cb_new_call(Fl_Widget* w, void*)
{
	Fl_Input* in = static_cast<Fl_Input*>(w);
	std::string v = ProfileStore::normalize_callsign(in->value());
	if (v != in->value()) {
		int pos = in->insert_position();
		in->value(v.c_str());
		in->insert_position(pos);
	}
}

void cb_new_create(Fl_Widget*, void* arg)
{
	NewProfile* np = static_cast<NewProfile*>(arg);
	std::string call = ProfileStore::normalize_callsign(np->call->value());
	if (!ProfileStore::is_base_callsign(call)) {
		fl_alert(_("A profile must be a base callsign: letters and digits only, no '/'."));
		return;
	}
	std::error_code ec;
	if (std::filesystem::exists(ProfileStore::db_path(np->root, call), ec)) {
		fl_alert(_("Profile %s already exists."), call.c_str());
		return;
	}
	ProfileStore store;
	if (!store.open(np->root, call) || !store.set_note(np->note->value())) {
		fl_alert(_("Could not create profile %s:\n%s"), call.c_str(), store.last_error().c_str());
		return;
	}
	np->created = call;
	np->win->hide();
}

void cb_hide(Fl_Widget*, void* arg)
{
	static_cast<Fl_Window*>(arg)->hide();
}

std::string new_profile_dialog(const std::string& root)
{
	NewProfile np;
	np.root = root;
	np.win = new Fl_Double_Window(440, 150, _("New profile"));
	np.call = new Fl_Input(170, 15, 150, 25, _("Callsign:"));
	np.call->when(FL_WHEN_CHANGED);
	np.call->callback(cb_new_call);
	np.note = new Fl_Input(170, 50, 255, 25, _("Note:"));
	Fl_Button* cancel = new Fl_Button(225, 110, 95, 25, _("Cancel"));
	cancel->callback(cb_hide, np.win);
	Fl_Return_Button* create = new Fl_Return_Button(330, 110, 95, 25, _("Create"));
	create->callback(cb_new_create, &np);
	np.win->end();
	np.win->set_modal();
	np.win->show();
	while (np.win->shown())
		Fl::wait();
	delete np.win;
	return np.created;
}

// ----------------------------------------------------------------------------
// Profile selection window

struct SelectProfile {
	Fl_Double_Window*       win;
	Fl_Hold_Browser*        list;
	std::string             root;
	std::vector<ProfileRow> rows;
	std::string             chosen;
};

void fill_list(SelectProfile* sp, const std::string& select)
{
	sp->rows = list_profiles(sp->root);
	sp->list->clear();
	int line = 0;
	for (size_t i = 0; i < sp->rows.size(); i++) {
		const ProfileRow& r = sp->rows[i];
		std::string text = r.callsign + "\t" + std::to_string(r.qsos) + "\t" + r.note;
		sp->list->add(text.c_str());
		if (r.callsign == select)
			line = static_cast<int>(i) + 1;
	}
	if (line == 0 && !sp->rows.empty())
		line = 1;
	if (line)
		sp->list->select(line);
}

void open_selected(SelectProfile* sp)
{
	int line = sp->list->value();
	if (line < 1 || line > static_cast<int>(sp->rows.size())) {
		fl_alert(_("Select a profile or create a new one."));
		return;
	}
	sp->chosen = sp->rows[line - 1].callsign;
	sp->win->hide();
}

void cb_open(Fl_Widget*, void* arg)
{
	open_selected(static_cast<SelectProfile*>(arg));
}

void cb_list(Fl_Widget*, void* arg)
{
	if (Fl::event_clicks())
		open_selected(static_cast<SelectProfile*>(arg));
}

void cb_new(Fl_Widget*, void* arg)
{
	SelectProfile* sp = static_cast<SelectProfile*>(arg);
	std::string call = new_profile_dialog(sp->root);
	if (!call.empty())
		fill_list(sp, call);
}

void cb_cancel(Fl_Widget*, void* arg)
{
	SelectProfile* sp = static_cast<SelectProfile*>(arg);
	sp->chosen.clear();
	sp->win->hide();
}

} // namespace

std::string profile_select_dialog(const std::string& root)
{
	std::error_code ec;
	std::filesystem::create_directories(root, ec);

	SelectProfile sp;
	sp.root = root;
	sp.win = new Fl_Double_Window(520, 320, _("Select profile"));

	std::string head_text = std::string(_("Callsign")) + "\t" + _("QSOs") + "\t" + _("Note");

	static int widths[] = { 150, 80, 0 };
	Fl_Hold_Browser* header = new Fl_Hold_Browser(10, 10, 500, 22);
	header->column_widths(widths);
	header->column_char('\t');
	header->format_char(0);
	header->has_scrollbar(0);
	header->add(head_text.c_str());
	header->deactivate();

	sp.list = new Fl_Hold_Browser(10, 32, 500, 233);
	sp.list->column_widths(widths);
	sp.list->column_char('\t');
	sp.list->format_char(0);
	sp.list->callback(cb_list, &sp);

	Fl_Button* bnew = new Fl_Button(10, 280, 110, 28, _("New profile"));
	bnew->callback(cb_new, &sp);
	Fl_Button* bcancel = new Fl_Button(300, 280, 100, 28, _("Cancel"));
	bcancel->callback(cb_cancel, &sp);
	Fl_Return_Button* bopen = new Fl_Return_Button(410, 280, 100, 28, _("Open"));
	bopen->callback(cb_open, &sp);

	sp.win->end();
	sp.win->callback(cb_cancel, &sp);
	fill_list(&sp, std::string());
	sp.win->show();
	while (sp.win->shown())
		Fl::wait();
	delete sp.win;
	return sp.chosen;
}
