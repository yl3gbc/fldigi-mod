// ----------------------------------------------------------------------------
// profile_radio.cxx
//
// Radio tab of the profile edit window (see MODERNIZATION.md).
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
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Input_Choice.H>
#include <FL/Fl_Round_Button.H>
#include <FL/Fl_Spinner.H>

#if USE_HAMLIB
#include <hamlib/rig.h>
#endif

#include "configuration.h"
#include "gettext.h"
#include "profile_radio.h"
#include "profile_store.h"
#include "xmlrpcpp/XmlRpc.h"

namespace {

enum RigKind { RIG_KIND_NONE = -1, RIG_KIND_FLRIG = -2, RIG_KIND_OTHER = -3 };
enum PttKind { PTT_VOX, PTT_CAT, PTT_DTR, PTT_RTS, PTT_OTHER };

const char* const baud_rates[] = {
	"300", "600", "1200", "2400", "4800", "9600", "19200",
	"38400", "57600", "115200", "230400", "460800"
};
const int n_baud_rates = sizeof(baud_rates) / sizeof(*baud_rates);

} // namespace

struct RadioTab {
	Fl_Group*        group;
	Fl_Choice*       rig;
	Fl_Input_Choice* cat_port;
	Fl_Choice*       baud;
	Fl_Input*        flrig_ip;
	Fl_Input*        flrig_port;
	Fl_Button*       test_cat;
	Fl_Box*          cat_result;
	Fl_Round_Button* ptt[4];
	Fl_Input_Choice* ptt_port;
	Fl_Spinner*      tx_delay;
	Fl_Spinner*      pretone;
	Fl_Button*       test_ptt;
	Fl_Box*          ptt_result;
	bool             other_rig;    // RigCAT or XML-RPC set in Configure
	bool             other_ptt;    // parallel port, uHRouter, CMedia or RigCAT PTT
	std::vector<std::pair<long, int> > models;   // Hamlib model, menu index
	std::string      cat_text;
	std::string      ptt_text;
};

namespace {

// ----------------------------------------------------------------------------
// Helpers

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

bool flag(ProfileStore& store, const char* tag, bool fallback)
{
	return atoi(setting_or(store, tag, fallback ? "1" : "0").c_str()) != 0;
}

// Serial ports: stable names first, then the kernel names.
std::vector<std::string> serial_ports()
{
	std::vector<std::string> ports;
	std::error_code ec;
	std::filesystem::directory_iterator it("/dev/serial/by-id", ec), end;
	for (; !ec && it != end; it.increment(ec))
		ports.push_back(it->path().string());
	std::sort(ports.begin(), ports.end());
	std::vector<std::string> tty;
	std::filesystem::directory_iterator dev("/dev", ec);
	for (; !ec && dev != end; dev.increment(ec)) {
		std::string name = dev->path().filename().string();
		if (name.compare(0, 6, "ttyUSB") == 0 || name.compare(0, 6, "ttyACM") == 0)
			tty.push_back(dev->path().string());
	}
	std::sort(tty.begin(), tty.end());
	ports.insert(ports.end(), tty.begin(), tty.end());
	return ports;
}

void fill_ports(Fl_Input_Choice* in, const std::string& value)
{
	std::vector<std::string> ports = serial_ports();
	for (size_t i = 0; i < ports.size(); i++)
		in->menubutton()->add(ports[i].c_str(), 0, 0, 0, 0);
	in->value(value.c_str());
}

// Escape a label for an FLTK menu path.
std::string menu_escape(const char* s)
{
	std::string out;
	for (; *s; s++) {
		if (*s == '/' || *s == '\\')
			out += '\\';
		else if (*s == '&')
			out += '&';
		else if (*s == '_' && out.empty())
			out += ' ';
		out += *s;
	}
	return out;
}

long selected_rig(const RadioTab* t)
{
	const Fl_Menu_Item* m = t->rig->mvalue();
	return m ? reinterpret_cast<long>(m->user_data()) : static_cast<long>(RIG_KIND_NONE);
}

int selected_ptt(const RadioTab* t)
{
	for (int i = 0; i < 4; i++)
		if (t->ptt[i]->value())
			return i;
	return PTT_OTHER;
}

bool is_hamlib(long rig)
{
	return rig > 0;
}

void show_result(Fl_Box* box, std::string& text, const std::string& value, bool ok)
{
	text = value;
	box->label(text.c_str());
	box->labelcolor(ok ? FL_DARK_GREEN : FL_RED);
	box->redraw();
	Fl::check();
}

// Keep the UI alive while PTT is held.
void hold(double seconds)
{
	double left = seconds;
	while (left > 0) {
		Fl::wait(0.05);
		left -= 0.05;
	}
}

// ----------------------------------------------------------------------------
// Rig list

#if USE_HAMLIB
struct RigEntry {
	long        model;
	std::string mfg;
	std::string name;
	std::string label;
};

int add_rig(const struct rig_caps* caps, void* arg)
{
	std::vector<RigEntry>* rigs = static_cast<std::vector<RigEntry>*>(arg);
	RigEntry e;
	e.model = caps->rig_model;
	e.mfg = caps->mfg_name;
	e.name = caps->model_name;
	// Same text fldigi's Configure uses for HAMRIGNAME.
	e.label = e.mfg + " " + e.name + "  (" + rig_strstatus(caps->status) + ")";
	rigs->push_back(e);
	return 1;
}

bool rig_less(const RigEntry& a, const RigEntry& b)
{
	int c = strcasecmp(a.mfg.c_str(), b.mfg.c_str());
	if (c != 0)
		return c < 0;
	c = strcasecmp(a.name.c_str(), b.name.c_str());
	if (c != 0)
		return c < 0;
	return a.model < b.model;
}

const std::vector<RigEntry>& hamlib_rig_list()
{
	static std::vector<RigEntry> rigs;
	if (rigs.empty()) {
		rig_set_debug(RIG_DEBUG_NONE);
		rig_load_all_backends();
		rig_list_foreach(add_rig, &rigs);
		std::sort(rigs.begin(), rigs.end(), rig_less);
	}
	return rigs;
}

const RigEntry* find_rig(long model)
{
	const std::vector<RigEntry>& rigs = hamlib_rig_list();
	for (size_t i = 0; i < rigs.size(); i++)
		if (rigs[i].model == model)
			return &rigs[i];
	return 0;
}
#endif

void update_states(RadioTab* t)
{
	long rig = selected_rig(t);
	bool hamlib = is_hamlib(rig);
	bool flrig = rig == RIG_KIND_FLRIG;
	bool serial = hamlib;
#if USE_HAMLIB
	if (hamlib) {
		const struct rig_caps* caps = rig_get_caps(static_cast<rig_model_t>(rig));
		serial = caps && caps->port_type == RIG_PORT_SERIAL;
	}
#endif
	if (hamlib) t->cat_port->activate(); else t->cat_port->deactivate();
	if (serial) t->baud->activate(); else t->baud->deactivate();
	if (flrig) { t->flrig_ip->activate(); t->flrig_port->activate(); }
	else { t->flrig_ip->deactivate(); t->flrig_port->deactivate(); }
	if (hamlib || flrig) t->test_cat->activate(); else t->test_cat->deactivate();

	if (hamlib || flrig)
		t->ptt[PTT_CAT]->activate();
	else {
		if (t->ptt[PTT_CAT]->value()) {
			t->ptt[PTT_CAT]->value(0);
			t->ptt[PTT_VOX]->value(1);
		}
		t->ptt[PTT_CAT]->deactivate();
	}
	int ptt = selected_ptt(t);
	if (ptt == PTT_DTR || ptt == PTT_RTS) t->ptt_port->activate(); else t->ptt_port->deactivate();
	if (ptt == PTT_VOX || ptt == PTT_OTHER) t->test_ptt->deactivate(); else t->test_ptt->activate();
}

void cb_changed(Fl_Widget*, void* arg)
{
	update_states(static_cast<RadioTab*>(arg));
}

// ----------------------------------------------------------------------------
// Tests

#if USE_HAMLIB
RIG* open_hamlib(const RadioTab* t, std::string& error)
{
	long model = selected_rig(t);
	RIG* rig = rig_init(static_cast<rig_model_t>(model));
	if (!rig) {
		error = _("Hamlib cannot use this rig model.");
		return 0;
	}
	rig_set_conf(rig, rig_token_lookup(rig, "rig_pathname"), t->cat_port->value());
	if (rig->caps->port_type == RIG_PORT_SERIAL) {
		int b = t->baud->value();
		if (b >= 0 && b < n_baud_rates)
			rig_set_conf(rig, rig_token_lookup(rig, "serial_speed"), baud_rates[b]);
	}
	int ret = rig_open(rig);
	if (ret != RIG_OK) {
		error = rigerror(ret);
		rig_cleanup(rig);
		return 0;
	}
	return rig;
}

void close_hamlib(RIG* rig)
{
	rig_close(rig);
	rig_cleanup(rig);
}
#endif

bool flrig_call(const RadioTab* t, const char* method, const XmlRpc::XmlRpcValue& params,
		XmlRpc::XmlRpcValue& result, std::string& error)
{
	XmlRpc::XmlRpcClient client(t->flrig_ip->value(), atoi(t->flrig_port->value()));
	if (!client.execute(method, params, result, 2.0) || client.isFault()) {
		error = _("No answer from flrig.");
		return false;
	}
	return true;
}

std::string freq_text(double hz)
{
	char buf[32];
	long long f = static_cast<long long>(hz + 0.5);
	snprintf(buf, sizeof(buf), "%lld.%03lld.%03lld Hz", f / 1000000, (f / 1000) % 1000, f % 1000);
	return buf;
}

void cb_test_cat(Fl_Widget*, void* arg)
{
	RadioTab* t = static_cast<RadioTab*>(arg);
	long rig = selected_rig(t);
	std::string error;
	show_result(t->cat_result, t->cat_text, _("Testing..."), true);
	if (rig == RIG_KIND_FLRIG) {
		XmlRpc::XmlRpcValue result;
		if (flrig_call(t, "rig.get_vfo", XmlRpc::XmlRpcValue(), result, error)) {
			std::string hz = static_cast<std::string>(result);
			show_result(t->cat_result, t->cat_text, freq_text(atof(hz.c_str())), true);
			return;
		}
	}
#if USE_HAMLIB
	else if (is_hamlib(rig)) {
		RIG* r = open_hamlib(t, error);
		if (r) {
			freq_t f = 0;
			int ret = rig_get_freq(r, RIG_VFO_CURR, &f);
			close_hamlib(r);
			if (ret == RIG_OK) {
				show_result(t->cat_result, t->cat_text, freq_text(f), true);
				return;
			}
			error = rigerror(ret);
		}
	}
#endif
	show_result(t->cat_result, t->cat_text, error, false);
}

bool serial_line(const char* port, int line, std::string& error, int& fd)
{
	fd = open(port, O_RDWR | O_NOCTTY | O_NONBLOCK);
	if (fd < 0) {
		error = std::string(_("Cannot open ")) + port;
		return false;
	}
	if (ioctl(fd, TIOCMBIS, &line) < 0) {
		error = std::string(_("Cannot set PTT line on ")) + port;
		close(fd);
		fd = -1;
		return false;
	}
	return true;
}

void cb_test_ptt(Fl_Widget*, void* arg)
{
	RadioTab* t = static_cast<RadioTab*>(arg);
	const double seconds = 1.5;
	int ptt = selected_ptt(t);
	long rig = selected_rig(t);
	std::string error;

	if (ptt == PTT_DTR || ptt == PTT_RTS) {
		int line = ptt == PTT_DTR ? TIOCM_DTR : TIOCM_RTS;
		int fd = -1;
		if (serial_line(t->ptt_port->value(), line, error, fd)) {
			show_result(t->ptt_result, t->ptt_text, _("PTT ON"), true);
			hold(seconds);
			ioctl(fd, TIOCMBIC, &line);
			close(fd);
			show_result(t->ptt_result, t->ptt_text, _("PTT OFF, test done"), true);
			return;
		}
	}
	else if (ptt == PTT_CAT && rig == RIG_KIND_FLRIG) {
		XmlRpc::XmlRpcValue result;
		if (flrig_call(t, "rig.set_ptt", XmlRpc::XmlRpcValue(1), result, error)) {
			show_result(t->ptt_result, t->ptt_text, _("PTT ON"), true);
			hold(seconds);
			flrig_call(t, "rig.set_ptt", XmlRpc::XmlRpcValue(0), result, error);
			show_result(t->ptt_result, t->ptt_text, _("PTT OFF, test done"), true);
			return;
		}
	}
#if USE_HAMLIB
	else if (ptt == PTT_CAT && is_hamlib(rig)) {
		RIG* r = open_hamlib(t, error);
		if (r) {
			int ret = rig_set_ptt(r, RIG_VFO_CURR, RIG_PTT_ON);
			if (ret == RIG_OK) {
				show_result(t->ptt_result, t->ptt_text, _("PTT ON"), true);
				hold(seconds);
				rig_set_ptt(r, RIG_VFO_CURR, RIG_PTT_OFF);
				close_hamlib(r);
				show_result(t->ptt_result, t->ptt_text, _("PTT OFF, test done"), true);
				return;
			}
			error = rigerror(ret);
			close_hamlib(r);
		}
	}
#endif
	show_result(t->ptt_result, t->ptt_text, error, false);
}

} // namespace

// ----------------------------------------------------------------------------

RadioTab* radio_tab_create(int x, int y, int w, int h, ProfileStore& store)
{
	RadioTab* t = new RadioTab;
	const configuration& d = progdefaults;

	t->group = new Fl_Group(x, y, w, h, _("Radio"));

	t->rig = new Fl_Choice(x + 140, y + 15, 340, 25, _("Rig:"));
	int none = t->rig->add(_("No CAT"), 0, 0, reinterpret_cast<void*>(static_cast<long>(RIG_KIND_NONE)));
	int flrig = t->rig->add("flrig", 0, 0, reinterpret_cast<void*>(static_cast<long>(RIG_KIND_FLRIG)));
	t->other_rig = flag(store, "CHKUSERIGCATIS", d.chkUSERIGCATis) ||
		       flag(store, "CHKUSEXMLRPCIS", d.chkUSEXMLRPCis);
	int other = -1;
	if (t->other_rig)
		other = t->rig->add(_("Other (set in Configure)"), 0, 0,
				    reinterpret_cast<void*>(static_cast<long>(RIG_KIND_OTHER)));
#if USE_HAMLIB
	const std::vector<RigEntry>& rigs = hamlib_rig_list();
	for (size_t i = 0; i < rigs.size(); i++) {
		std::string path = menu_escape(rigs[i].mfg.c_str()) + "/" +
				   menu_escape((rigs[i].mfg + " " + rigs[i].name).c_str());
		int idx = t->rig->add(path.c_str(), 0, 0, reinterpret_cast<void*>(rigs[i].model));
		t->models.push_back(std::make_pair(rigs[i].model, idx));
	}
#endif
	long model = atol(setting_or(store, "HAMRIGMODEL", to_str(d.HamRigModel)).c_str());
	int select = none;
	if (t->other_rig)
		select = other;
	else if (flag(store, "CHKUSEHAMLIBIS", d.chkUSEHAMLIBis)) {
		for (size_t i = 0; i < t->models.size(); i++)
			if (t->models[i].first == model)
				select = t->models[i].second;
	}
	else if (flag(store, "CLIENT_TO_FLRIG", d.fldigi_client_to_flrig))
		select = flrig;
	t->rig->value(select);
	t->rig->callback(cb_changed, t);

	t->cat_port = new Fl_Input_Choice(x + 140, y + 50, 200, 25, _("CAT port:"));
	fill_ports(t->cat_port, setting_or(store, "HAMRIGDEVICE", d.HamRigDevice));
	t->baud = new Fl_Choice(x + 410, y + 50, 90, 25, _("Baud:"));
	for (int i = 0; i < n_baud_rates; i++)
		t->baud->add(baud_rates[i]);
	int baud = atoi(setting_or(store, "HAMRIGBAUDRATE", to_str(d.HamRigBaudrate)).c_str());
	t->baud->value(baud >= 0 && baud < n_baud_rates ? baud : 5);

	t->flrig_ip = new Fl_Input(x + 140, y + 85, 160, 25, _("flrig address:"));
	t->flrig_ip->value(setting_or(store, "FLRIG_IP_ADDRESS", d.flrig_ip_address).c_str());
	t->flrig_port = new Fl_Input(x + 410, y + 85, 90, 25, _("Port:"));
	t->flrig_port->value(setting_or(store, "FLRIG_IP_PORT", d.flrig_ip_port).c_str());

	t->test_cat = new Fl_Button(x + 140, y + 120, 100, 25, _("Test CAT"));
	t->test_cat->callback(cb_test_cat, t);
	t->cat_result = new Fl_Box(x + 250, y + 120, 250, 25);
	t->cat_result->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

	Fl_Group* ptt = new Fl_Group(x + 140, y + 165, 360, 25, _("PTT:"));
	ptt->align(FL_ALIGN_LEFT);
	const char* ptt_names[4] = { "VOX", "CAT", "DTR", "RTS" };
	for (int i = 0; i < 4; i++) {
		t->ptt[i] = new Fl_Round_Button(x + 140 + i * 75, y + 165, 70, 25, ptt_names[i]);
		t->ptt[i]->type(FL_RADIO_BUTTON);
		t->ptt[i]->callback(cb_changed, t);
	}
	ptt->end();

	bool tty = flag(store, "TTYPTT", d.TTYptt);
	bool dtr = flag(store, "DTRPTT", d.DTRptt);
	bool rts = flag(store, "RTSPTT", d.RTSptt);
	bool hamlib_ptt = flag(store, "HAMLIBCMDPTT", d.HamlibCMDptt);
	t->other_ptt = flag(store, "USEPPORTPTT", d.UsePPortPTT) ||
		       flag(store, "USEUHROUTERPTT", d.UseUHrouterPTT) ||
		       flag(store, "CMEDIA-PTT", d.cmedia_ptt) ||
		       (t->other_rig && flag(store, "RIGCATCMDPTT", d.RigCatCMDptt));
	long rig = selected_rig(t);
	if (tty && rts && !dtr)
		t->ptt[PTT_RTS]->value(1);
	else if (tty)
		t->ptt[PTT_DTR]->value(1);
	else if ((hamlib_ptt && is_hamlib(rig)) || rig == RIG_KIND_FLRIG)
		t->ptt[PTT_CAT]->value(1);
	else if (!t->other_ptt)
		t->ptt[PTT_VOX]->value(1);

	t->ptt_port = new Fl_Input_Choice(x + 140, y + 200, 200, 25, _("PTT port:"));
	fill_ports(t->ptt_port, setting_or(store, "PTTDEV", d.PTTdev));

	t->tx_delay = new Fl_Spinner(x + 140, y + 235, 80, 25, _("Tx delay (ms):"));
	t->tx_delay->type(FL_INT_INPUT);
	t->tx_delay->range(0, 2000);
	t->tx_delay->step(10);
	t->tx_delay->value(atoi(setting_or(store, "PTTONDELAY", to_str(d.PTT_on_delay)).c_str()));
	t->pretone = new Fl_Spinner(x + 410, y + 235, 90, 25, _("Tone before Tx (s):"));
	t->pretone->type(FL_FLOAT_INPUT);
	t->pretone->range(0, 10);
	t->pretone->step(0.1);
	t->pretone->format("%.1f");
	// Stored with the C locale ("0.5"), so not atof(), which follows LC_NUMERIC.
	double pretone = 0;
	std::istringstream pretone_in(setting_or(store, "PRETONE", to_str(d.pretone)));
	pretone_in >> pretone;
	t->pretone->value(pretone);

	t->test_ptt = new Fl_Button(x + 140, y + 270, 100, 25, _("Test PTT"));
	t->test_ptt->callback(cb_test_ptt, t);
	t->ptt_result = new Fl_Box(x + 250, y + 270, 250, 25);
	t->ptt_result->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

	t->group->end();
	update_states(t);
	return t;
}

bool radio_tab_save(const RadioTab* t, ProfileStore& s)
{
	long rig = selected_rig(t);
	bool ok = true;
	if (rig != RIG_KIND_OTHER) {
		ok = ok && s.set_setting("CHKUSERIGCATIS", "0") && s.set_setting("CHKUSEXMLRPCIS", "0");
		ok = ok && s.set_setting("CHKUSEHAMLIBIS", is_hamlib(rig) ? "1" : "0");
		ok = ok && s.set_setting("CLIENT_TO_FLRIG", rig == RIG_KIND_FLRIG ? "1" : "0");
	}
#if USE_HAMLIB
	if (is_hamlib(rig)) {
		const RigEntry* e = find_rig(rig);
		ok = ok && s.set_setting("HAMRIGMODEL", to_str(rig));
		ok = ok && s.set_setting("HAMRIGNAME", e ? e->label : std::string());
	}
#endif
	ok = ok && s.set_setting("HAMRIGDEVICE", t->cat_port->value());
	ok = ok && s.set_setting("HAMRIGBAUDRATE", to_str(t->baud->value()));
	ok = ok && s.set_setting("FLRIG_IP_ADDRESS", t->flrig_ip->value());
	ok = ok && s.set_setting("FLRIG_IP_PORT", t->flrig_port->value());

	int ptt = selected_ptt(t);
	if (ptt != PTT_OTHER) {
		ok = ok && s.set_setting("TTYPTT", ptt == PTT_DTR || ptt == PTT_RTS ? "1" : "0");
		ok = ok && s.set_setting("DTRPTT", ptt == PTT_DTR ? "1" : "0");
		ok = ok && s.set_setting("RTSPTT", ptt == PTT_RTS ? "1" : "0");
		ok = ok && s.set_setting("HAMLIBCMDPTT", ptt == PTT_CAT && is_hamlib(rig) ? "1" : "0");
		ok = ok && s.set_setting("RIGCATCMDPTT", "0");
		ok = ok && s.set_setting("USEPPORTPTT", "0");
		ok = ok && s.set_setting("USEUHROUTERPTT", "0");
		ok = ok && s.set_setting("CMEDIA-PTT", "0");
	}
	ok = ok && s.set_setting("PTTDEV", t->ptt_port->value());
	ok = ok && s.set_setting("PTTONDELAY", to_str(static_cast<int>(t->tx_delay->value())));
	ok = ok && s.set_setting("PRETONE", to_str(t->pretone->value()));
	return ok;
}

void radio_tab_delete(RadioTab* t)
{
	delete t;
}
