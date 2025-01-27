#include <ncurses.h>
#include <stdio.h>
#include <pty.h>
#include <spawn.h>
#include <unistd.h>
#include "windows.hpp"
#include "rwm.h"
#include "desktop.hpp"
#include "charencoding.hpp"

namespace rwm {
	const std::string version = "0.9";
	// Key Codes
	std::unordered_map<int, std::string> key_conversion = {
		// Normal
		{KEY_UP, "\033[A"},
		{KEY_DOWN, "\033[B"},
		{KEY_RIGHT, "\033[C"},
		{KEY_LEFT, "\033[D"},
		{KEY_BACKSPACE, "\b"},
		{KEY_HOME, "\033[1~"},
		{KEY_IC, "\033[2~"},
		{KEY_DL, "\033[3~"},
		{KEY_END, "\033[4~"},
		{KEY_PPAGE, "\033[5~"},
		{KEY_NPAGE, "\033[6~"},
		//{KEY_HOME, "\033[7~"},
		//{KEY_END, "\033[8~"},

		// Shift
		{KEY_SR, "\033[2A"},
		{KEY_SF, "\033[2B"},
		{KEY_SRIGHT, "\033[2C"},
		{KEY_SLEFT, "\033[2D"},
		//{KEY_BACKSPACE, "\b"},
		{KEY_SHOME, "\033[1;2~"},
		{KEY_SIC, "\033[2;2~"},
		{KEY_SDC, "\033[3;2~"},
		{KEY_SEND, "\033[4;2~"},
		{KEY_SPREVIOUS, "\033[5;2~"},
		{KEY_SNEXT, "\033[6;2~"},
		//{KEY_SHOME, "\033[7;2~"},
		//{KEY_SEND, "\033[8;2~"},

		// Ctrl
		{567, "\033[5A"},
		{526, "\033[5B"},
		{561, "\033[5C"},
		{546, "\033[5D"},
		//{KEY_BACKSPACE, "\b"},
		{536, "\033[1;5~"},
		//{KEY_SIC, "\033[5;5~"},
		//{KEY_SDC, "\033[3;5~"},
		{531, "\033[4;5~"},
		{556, "\033[5;5~"},
		{551, "\033[6;5~"},

		// Ctrl-Shift
		{568, "\033[6A"},
		{527, "\033[6B"},
		{547, "\033[6C"},
		{562, "\033[6D"},
		//{KEY_BACKSPACE, "\b"},
		{537, "\033[1;6~"},
		//{KEY_SIC, "\033[2;6~"},
		{521, "\033[3;6~"},
		{532, "\033[4;6~"},
		{557, "\033[5;6~"},
		{552, "\033[6;6~"},

		// F keys
		{264, "\033[10~"},
		{265, "\033[11~"},
		{266, "\033[12~"},
		{267, "\033[13~"},
		{268, "\033[14~"},
		{269, "\033[15~"},
		{270, "\033[17~"},
		{271, "\033[18~"},
		{272, "\033[19~"},
		{273, "\033[20~"},
		{274, "\033[21~"},
		{275, "\033[23~"},
		{276, "\033[24~"},
		{330, "\033[3~"},
	};
	std::unordered_map<int, std::string> app_key_conversion = {
		{KEY_UP, "\033OA"},
		{KEY_DOWN, "\033OB"},
		{KEY_RIGHT, "\033OC"},
		{KEY_LEFT, "\033OD"},
		{KEY_HOME, "\033OH"},
		{KEY_END, "\033OF"},

		// Shift
		{KEY_SR, "\033O2A"},
		{KEY_SF, "\033O2B"},
		{KEY_SRIGHT, "\033O2C"},
		{KEY_SLEFT, "\033O2D"},
		{KEY_SHOME, "\033O2H"},
		{KEY_SEND, "\033O2F"},

		// Ctrl
		{567, "\033O5A"},
		{526, "\033O5B"},
		{561, "\033O5C"},
		{546, "\033O5D"},
		{536, "\033O5H"},
		{531, "\033O5F"},

		// Ctrl-Shift
		{568, "\033O6A"},
		{527, "\033O6B"},
		{547, "\033O6C"},
		{562, "\033O6D"},
		{537, "\033O6H"},
		{532, "\033O6F"},
	};
	std::unordered_map<int, char> mouse_conversion = {
		{BUTTON1_PRESSED, 32},
		{BUTTON2_PRESSED, 33},
		{BUTTON3_PRESSED, 34},
		{BUTTON4_PRESSED, 96},
		{BUTTON5_PRESSED, 97},
		{BUTTON1_RELEASED, 35},
		{BUTTON2_RELEASED, 35},
		{BUTTON3_RELEASED, 35},
		{BUTTON4_RELEASED, 99},
		{BUTTON5_RELEASED, 99},
	};

	void terminate() {
		rwm_desktop::terminate();
		debug_log.close();
		echo();
		if (has_colors())
			use_default_colors();
		endwin();
		exit(0);
	}

	int spawn(std::vector<std::string> args) {
		pid_t processID;
		std::vector<char*> c_args;
		for (std::string const& a : args)
			c_args.emplace_back(const_cast<char*>(a.c_str()));
		c_args.push_back(nullptr);

		return posix_spawn(&processID, c_args[0], nullptr, nullptr, &c_args[0], environ);
	}

	void init() {
		setlocale(LC_CTYPE, "");
		initscr();
		init_encoding();
		cbreak();
		noecho();
		intrflush(stdscr, FALSE);
		keypad(stdscr, TRUE);
		mousemask(MOUSE_MASK, NULL);
		nonl();
		raw();
		set_escdelay(0);
		mouseinterval(0);
		timeout(0);
		bold_mode = BOLD;

		if (has_colors()) {
			start_color();
			use_default_colors();
			assume_default_colors((uint32_t) DEFAULT_COLOR, (uint32_t) (DEFAULT_COLOR >> 32));
			
			// Initialise bright colors
			base_colors = (COLORS < base_colors) ? COLORS : base_colors;
			max_colors = (COLORS < max_colors) ? COLORS : max_colors;
			max_color_pairs = (COLOR_PAIRS < max_color_pairs) ? COLOR_PAIRS : max_color_pairs;
			if (!HAS_EXT_COLOR) {
				max_colors = (256 < max_colors) ? 256 : max_colors;
				max_color_pairs = (256 < max_color_pairs) ? 256 : max_color_pairs;
			}

			for (short i = 8; i < base_colors; i++) {
				init_color(i, (i & 1) ? 1000 : 500, (i & 2) ? 1000 : 500, (i & 4) ? 1000 : 500);
			}
			colors = base_colors;

			for (int i = -1; i < colors; i++) {
				color_map.insert_or_assign(i, i);
			}

			if (colors < 16) {
				for (int i = colors; i < 16; i++) {
					color_map.insert_or_assign(i, i - colors);
				}
			}

			pair_map.insert_or_assign(DEFAULT_COLOR, COLOR_PAIR(0));
		}
		rwm_desktop::init();
		rwm_desktop::render();
	}

	void move_to_top(int i) {
		if (windows[SEL_WIN]->status & REPORT_FOCUS)
			windows[SEL_WIN]->send("\033[O");

		Window* sel = windows[i];
		windows.erase(windows.begin() + i);
		windows.push_back(sel);

		if (windows[SEL_WIN]->status & REPORT_FOCUS)
			windows[SEL_WIN]->send("\033[I");
		windows[SEL_WIN]->should_refresh = true;
	}

	void set_selected(int i) {
		windows[SEL_WIN]->render(false);
		if (i < 0) {
			selected_window = false;
			return;
		}
		windows[i]->render(true);
		move_to_top(i);
		selected_window = true;
	}

	int get_top_window(ivec2 pos) {
		for (int i = windows.size() - 1; i >= 0; i--) {
			int y, x, maxy, maxx;
			getbegyx(windows[i]->frame, y, x);
			getmaxyx(windows[i]->frame, maxy, maxx);
			if (!(windows[i]->status & HIDDEN) 
			 && x <= pos.x && pos.x < x + maxx
			 && y <= pos.y && pos.y < y + maxy)
			 	return i;
		}
		return -1;
	}

	bool is_on_frame(ivec2 pos) {
		WINDOW* f = windows[SEL_WIN]->frame;
		int y, x, maxy, maxx;
		getbegyx(f, y, x);
		getmaxyx(f, maxy, maxx);
		return x == pos.x || x + maxx - 1 == pos.x
		    || y == pos.y || y + maxy - 1 == pos.y;
	}

	void close_window(int i) {
		if (i < 0) 
			return;
		if (i == SEL_WIN) 
			set_selected(-1);
		
		if (windows[i]->destroy()) {
			rwm_desktop::close_window(windows[i]);
			delete windows[i];
			windows.erase(windows.begin() + i);
			full_refresh();
		} else if (DEBUG) {
			print_debug("Zombie process!");
		}
	}

	void full_refresh() {
		rwm_desktop::render();
		for (int i = 0; i < rwm::windows.size(); i++)
			rwm::windows[i]->render(i == SEL_WIN);
	}

	inline int main() {
		init();
		doupdate();
		if (DEBUG)
			debug_log << "==== RESTART ====\n";
		bool is_window_dragged = false;

		while (true) {
			bool should_refresh = rwm_desktop::update();
			if (should_refresh)
				rwm_desktop::render();
			for (int i = 0; i < windows.size(); i++) {
				if (!(windows[i]->status & FROZEN)) {
					int ret = windows[i]->output();
					should_refresh |= (ret == 1);
					if ((windows[i]->status & SHOULD_CLOSE) && !(windows[i]->status & NO_EXIT)) {
						close_window(i);
						should_refresh = true;
					} else if (should_refresh && !(windows[i]->status & HIDDEN))
						windows[i]->render(i == SEL_WIN);
				} else if ((windows[i]->status & FROZEN) && should_refresh)
					windows[i]->render(i == SEL_WIN);
			}

			if (SEL_WIN < 0)
				selected_window = false;
			if (should_refresh)
				doupdate();
			
			int c;
			if (selected_window)
				c = wgetch(windows[SEL_WIN]->win);
			else {
				c = getch();
				curs_set(0);
			}

			if (DEBUG && c != -1) {
				print_debug(keyname(c) + (" " +  (c != 13 && c != 10) ? std::to_string(c) : "\\n"));
			}
			if (rwm_desktop::key_priority(c)) 
				continue;
			
			MEVENT event;
			switch (c) {
			case KEY_MOUSE:
				if (getmouse(&event) == OK) {
					ivec2 click_pos = {event.y, event.x};
					if (SEL_WIN >= 0) {
						if (event.bstate & MOUSE_PRESSED) {
							set_selected(get_top_window(click_pos));
							if (is_on_frame(click_pos))
								is_window_dragged = rwm_desktop::frame_click(SEL_WIN, click_pos, event.bstate);
						} else if (event.bstate & MOUSE_RELEASED) {
							if (is_window_dragged)
								is_window_dragged = rwm_desktop::frame_click(SEL_WIN, click_pos, event.bstate);
							set_selected(get_top_window(click_pos));
						}
						if (selected_window) {
							switch (windows[SEL_WIN]->mouse_mode) {
								case 1000: {
									int x, y;
									getbegyx(windows[SEL_WIN]->win, y, x);
									std::string mouse_msg = {'\033', '[', 'M', mouse_conversion.at(event.bstate & MOUSE_MASK), 
										(char) (event.x - x + 33), (char) (event.y - y + 33)
									};
									if (DEBUG)
										print_debug(mouse_msg);
									windows[SEL_WIN]->send(mouse_msg);
								}
								break;

								/*case 1006:
								std::string mouse_msg = "\033[" + std::to_string(mouse_state & 3) + ';'
									+ std::to_string(event.y- y + 1) + ';' + std::to_string(event.x - x + 1)
									+ 'M';*/


								default:
								break;
							}
						} else
							rwm_desktop::mouse_pressed(event);
					} else
						rwm_desktop::mouse_pressed(event);
					move(event.y, event.x);
				}
				break;

			case -1:
				break;

			case KEY_BACKSPACE: case '\b':
				c = '\b';
				if (selected_window) {
					windows[SEL_WIN]->send(c);
				} else
					rwm_desktop::key_pressed(c);
				break;

			default:
				if (selected_window) {
					if ((windows[SEL_WIN]->status & APP_CURSOR) && app_key_conversion.find(c) != app_key_conversion.end()) {
						windows[SEL_WIN]->send(app_key_conversion.at(c));
					} else if (key_conversion.find(c) != key_conversion.end()) {
						windows[SEL_WIN]->send(key_conversion.at(c));
					} else if (c > 276 && c < 313) {
						int cc = (c - 265) % 12 + 265;
						int mod = (c - 265) / 12;
						int shift = mod & 1;
						int ctrl = (mod & 2) << 1;
						int alt = (mod & 4) >> 1;
						std::string code = key_conversion.at(cc).substr(0, 4) + ';' + std::to_string(shift + ctrl + alt) + '~';
					} else if (c < 256) {
						windows[SEL_WIN]->send(c);
					}
				} else
					rwm_desktop::key_pressed(c);
				break;
				
			case '\x03':
				// ^C
				if (selected_window)  {
					windows[SEL_WIN]->send(c);
					windows[SEL_WIN]->status &= ~NO_EXIT;
				} else
					rwm_desktop::key_pressed(c);
				break;
			}
			//usleep(20000);
		}
		terminate();
	}
};

int main(int argc, char* argv[]) {
	rwm_desktop::parse_args(argc, argv);
	return rwm::main();
}

