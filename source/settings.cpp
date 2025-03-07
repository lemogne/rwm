#include <fstream>
#include <unordered_set>
#include "rwmdesktop.hpp"
#include "rwm.h"

namespace rwm_settings {
	std::unordered_map<std::string, std::string*> string_vars = {
		{"desktop_directory", &rwm_desktop::desktop_path},
		{"cwd",               &rwm_desktop::cwd},
		{"default_shell",     &rwm_desktop::shell},

		{"dmenu",             &rwm_desktop::buttons[0]},
		{"window_menu",       &rwm_desktop::buttons[1]},
		{"task_tab",          &rwm_desktop::buttons[2]},
	};

	std::unordered_map<std::string, std::pair<std::string*, size_t>> utf8char_vec_vars = {
		{"frame_idle",   {&rwm_desktop::frame_chars[rwm_desktop::IDLE], 8}},
		{"frame_top",    {&rwm_desktop::frame_chars[rwm_desktop::ACTIVE], 8}},
		{"frame_sel",    {&rwm_desktop::frame_chars[rwm_desktop::SELECTED], 8}},
		{"frame_resize", {&rwm_desktop::frame_chars[rwm_desktop::RESIZE], 8}},
	};

	std::unordered_map<std::string, int*> color_vars = {
		{"taskbar",           &rwm_desktop::theme[0]},
		{"frame",             &rwm_desktop::frame_theme[0]},
	};

	std::unordered_map<std::string, std::vector<std::string>*> string_vec_vars = {
		{"background", &rwm_desktop::background_program},
	};
	

	std::unordered_map<std::string, std::pair<int*, size_t>> int_vars = {
		{"task_tab_size", {&rwm_desktop::tab_size, 1}},
		{"default_window_size", {&rwm_desktop::win_size.y, 2}},
		{"refresh_rate", {&rwm::sleep_time, 1}},
	};

	std::unordered_map<std::string, bool*> bool_vars = {
		{"draw_icons", &rwm_desktop::should_draw_icons},
		{"force_convert", &rwm::force_convert}
	};

	void set_str(std::unordered_map<const std::string, std::string*>::iterator it, std::string value) {
		if (value[0] == '~')
			value = getenv("HOME") + value.substr(1);
		*it->second = value;
	}

	void set_str_vec(std::unordered_map<const std::string, std::vector<std::string>*>::iterator it, std::string value) {
		if (value[0] == '~')
			value = getenv("HOME") + value.substr(1);
		std::istringstream ss(value, std::ios::in);
		for (std::string line; std::getline(ss, line, ' ');)
			it->second->push_back(line);
	}

	void set_color(std::unordered_map<const std::string, int*>::iterator it, std::string value) {
		int* p = it->second;
		int ctr = 0;
		enum state {
			NORMAL, BRACKET
		};

		state s = NORMAL;

		for (char& c : value) {
			if (s == NORMAL) {
				if ('0' <= c && c <= '9') {
					*p++ = c - '0';
					ctr++;
				} else if ('a' <= c && c <= 'f') {
					*p++ = c - 'a' + 10;
					ctr++;
				} else if (c == '*') {
					*p++ = -1;
				} else if (c == '[') {
					s = BRACKET;
					*p = 0;
				} else {
					rwm_desktop::show_info(std::string("Invalid color value ") + c + ".");
				}
			} else {
				if ('0' <= c && c <= '9') {
					*p *= 10;
					*p += c - '0';
				} else if (c == ']') {
					p++;
					ctr++;
					s = NORMAL;
				}
			}

			if (ctr >= 2)
				return;
		}
	}

	void set_int(std::unordered_map<const std::string, std::pair<int *, size_t>>::iterator it, std::string value) {
		int* p = it->second.first;
		size_t n = it->second.second;
		std::istringstream ss(value, std::ios::in);
		std::string line;
		for (int i = 0; i < n && std::getline(ss, line, ' '); i++) {
			for (char& c : line) {
				if (!std::isdigit(c)) {
					rwm_desktop::show_info(
						  "While parsing settings.cfg: Value '" 
						+ value 
						+ "' is not a valid non-negative integer constant for variable " 
						+ it->first
					);
					return;
				}
			}
			*p++ = atoi(line.c_str());
		}
	}

	void set_utf8ch_vec(std::unordered_map<const std::string, std::pair<std::string *, size_t>>::iterator it, std::string value) {
		std::string* p = it->second.first;
		size_t n = it->second.second;
		for (char& c : value) {
			*p += c;
			if (c >= 0) {
				p++;
				if (p - it->second.first > n * sizeof(std::string*)) {
					rwm_desktop::show_info(
						"While parsing Settings: Char Vector '" 
					  + it->first
					  + "' has length of at most " 
					  + std::to_string(n)
					  + ", yet the passed sequence '"
					  + value
					  + "' is longer."
				  	);
					break;
				}
			}
		}
	}

	void set_bool(std::unordered_map<const std::string, bool *>::iterator it, std::string value) {
		if (!value.compare("true"))
			*it->second = true;
		else if (!value.compare("false"))
			*it->second = false;
		else
			rwm_desktop::show_info(
				  "While parsing Settings: Value '" 
				+ value 
				+ "' is not a valid boolean constant for variable " 
				+ it->first
			);
	}

	void read_settings(std::string path) {
		std::ifstream settings(path, std::ios::in);
		if (!settings) {
			rwm_desktop::show_info("Settings file not found: " + path);
			return;
		}

		for (std::string line; std::getline(settings, line); ) {
			if (line.length() == 0 || line[0] == '#' || line[0] == '[')
				continue;
			size_t pos = line.find('=');
			if (pos == line.npos) 
				continue;

			std::string var = line.substr(0, pos);
			std::string value = line.substr(pos + 1);

			auto itstr = string_vars.find(var);
			auto itutf8chvec = utf8char_vec_vars.find(var);
			auto itstrvec = string_vec_vars.find(var);
			auto itint = int_vars.find(var);
			auto itbool = bool_vars.find(var);
			auto itcol = color_vars.find(var);

			if (itstr != string_vars.end())
				set_str(itstr, value);
			else if (itutf8chvec != utf8char_vec_vars.end())
				set_utf8ch_vec(itutf8chvec, value);
			else if (itstrvec != string_vec_vars.end())
				set_str_vec(itstrvec, value);
			else if (itint != int_vars.end())
				set_int(itint, value);
			else if (itbool != bool_vars.end())
				set_bool(itbool, value);
			else if (itcol != color_vars.end())
				set_color(itcol, value);
			else
				rwm_desktop::show_info("Settings: Variable not found: " + var);
		}	
	}

	void read_envvars(std::string path) {
		std::ifstream envvars(path, std::ios::in);
		if (!envvars) {
			rwm_desktop::show_info("Environment variables file not found: " + path);
			return;
		}

		for (std::string line; std::getline(envvars, line); ) {
			if (line.length() == 0 || line[0] == '#' || line[0] == '[')
				continue;
			size_t pos = line.find('=');
			if (pos == line.npos) 
				continue;
			
			std::string var = line.substr(0, pos);
			std::string rawvalue = line.substr(pos + 1);
			if (rawvalue[0] == '~')
				rawvalue = getenv("HOME") + rawvalue.substr(1);

			std::string value;
			size_t pvpos = 0;
			size_t vpos;
			for(vpos = rawvalue.find('$', 0); vpos != rawvalue.npos; vpos = rawvalue.find('$', vpos + 1)) {
				value += rawvalue.substr(pvpos, vpos - pvpos);
				pvpos = ++vpos;
				while(vpos < rawvalue.length() && (isalnum(rawvalue[vpos]) || rawvalue[vpos] == '_'))
					vpos++;
				std::string varname = rawvalue.substr(pvpos, vpos - pvpos);
				if (char* envval = getenv(varname.c_str()))
					value += envval;
				pvpos = vpos;
			}
			value += rawvalue.substr(pvpos, vpos - pvpos);

			setenv(var.c_str(), value.c_str(), true);
		}	
	}
};
