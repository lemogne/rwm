#include <fstream>
#include "rwmdesktop.hpp"
#include "rwm.h"

namespace rwm_settings {
	std::unordered_map<std::string, std::string*> string_vars = {
		{"desktop_directory", &rwm_desktop::desktop_path},
		{"cwd",               &rwm_desktop::cwd},
		{"default_shell",     &rwm_desktop::shell},

		{"frame_vert_idle",   &rwm_desktop::frame_chars[rwm_desktop::IDLE]},
		{"frame_horz_idle",   &rwm_desktop::frame_chars[rwm_desktop::IDLE + 1]},
		{"frame_vert_top",    &rwm_desktop::frame_chars[rwm_desktop::TOP]},
		{"frame_horz_top",    &rwm_desktop::frame_chars[rwm_desktop::TOP + 1]},
		{"frame_vert_sel",    &rwm_desktop::frame_chars[rwm_desktop::SELECTED]},
		{"frame_horz_sel",    &rwm_desktop::frame_chars[rwm_desktop::SELECTED + 1]},
		{"frame_vert_resize", &rwm_desktop::frame_chars[rwm_desktop::RESIZE]},
		{"frame_horz_resize", &rwm_desktop::frame_chars[rwm_desktop::RESIZE + 1]},

		{"dmenu",             &rwm_desktop::buttons[0]},
		{"window_menu",       &rwm_desktop::buttons[1]},
		{"task_tab",          &rwm_desktop::buttons[2]},
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
		{"default_window_size", {&rwm_desktop::win_size.y, 2}}
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

	void set_int(std::unordered_map<const std::string, std::pair<int *, size_t>>::iterator it, std::string value) {
		int* p = it->second.first;
		size_t n = it->second.second;
		std::istringstream ss(value, std::ios::in);
		std::string line;
		for (int i = 0; i < n && std::getline(ss, line, ' '); i++) {
			for (char& c : line) {
				if (!std::isdigit(c)) {
					rwm::print_debug(
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

	void set_bool(std::unordered_map<const std::string, bool *>::iterator it, std::string value) {
		if (!value.compare("true"))
			*it->second = true;
		else if (!value.compare("false"))
			*it->second = false;
		else
			rwm::print_debug(
				  "While parsing settings.cfg: Value '" 
				+ value 
				+ "' is not a valid boolean constant for variable " 
				+ it->first
			);
	}

	void read_settings(std::string path) {
		std::ifstream settings(path, std::ios::in);
		if (!settings) {
			rwm::print_debug("Settings file not found: " + path);
			return;
		}

		for (std::string line; std::getline(settings, line); ) {
			if (line.length() == 0 || line[0] == '#' || line[0] == '[')
				continue;
			size_t pos = line.find('=');

			std::string var = line.substr(0, pos);
			std::string value = line.substr(pos + 1);

			auto itstr = string_vars.find(var);
			auto itstrvec = string_vec_vars.find(var);
			auto itint = int_vars.find(var);
			auto itbool = bool_vars.find(var);

			if (itstr != string_vars.end())
				set_str(itstr, value);
			else if (itstrvec != string_vec_vars.end())
				set_str_vec(itstrvec, value);
			else if (itint != int_vars.end())
				set_int(itint, value);
			else if (itbool != bool_vars.end())
				set_bool(itbool, value);
			else
				rwm::print_debug("Variable not found: " + var);
		}	
	}
};
