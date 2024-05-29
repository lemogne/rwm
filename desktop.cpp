#include "desktop.hpp"
#include <termios.h>
#include <fcntl.h>
#include <pty.h>
#include <iomanip>
#include <ctime>
#include <sstream>
#include <fstream>
#include <dirent.h>
#include "rwm.h"
#include "charencoding.hpp"

namespace rwm_desktop {
	rwm::ivec2 drag_pos = {-1, -1};
	bool should_refresh = false;
	bool alt_pressed = false;
	std::string desktop_path = getenv("HOME") + std::string("/Desktop/");
	std::vector<std::string> desktop_contents = {};
	int tab_size = 20;
	rwm::ivec2 spacing = {6, 10};
	rwm::ivec2 click = {-1, -1};

	char icons[2][3][11] = {
		{
			"██╲",
			"███",
			"███",
		},
		{
			"┌┐_",
			"╲██",
			"    ",
		}
	};

	void draw_time() {
		attron(A_REVERSE);
		time_t t = std::time(nullptr);
		tm tm = *std::localtime(&t);
		std::ostringstream oss;
		oss << std::put_time(&tm, "%H:%M");
		mvaddstr(stdscr->_maxy, stdscr->_maxx - 5, oss.str().c_str());
		attroff(A_REVERSE);
	}

	void draw_taskbar() {
		std::string bar = std::string(stdscr->_maxx + 1, ' ');
		attron(A_REVERSE);
		mvaddstr(stdscr->_maxy, 0, bar.c_str());
		mvaddstr(stdscr->_maxy, 0, "[rwm]");
		for (int i = 0; i <= SEL_WIN; i++) {
			rwm::Window& win = rwm::windows[i];
			if (i == SEL_WIN && rwm::selected_window) 
				attroff(A_REVERSE);
			else
				attron(A_REVERSE);
			std::string display_title;
			if (win.title.length() < tab_size) {
				display_title = win.title + std::string(tab_size - win.title.length(), ' ');
			} else {
				display_title = win.title.substr(0, tab_size - 3) + "...";
			}
			addstr(('[' + display_title + ']').c_str());
		}
		draw_time();
		attroff(A_REVERSE);
	}

	void draw_background(std::string filename, rwm::ivec2 pos, rwm::ivec2 size) {
		// Temporary solution; will write proper parser later
		std::ifstream file;
		std::string contents;
		std::string out;
		file.open(filename);
		int y = pos.y;
		move(y, pos.x);
		refresh();
		while (getline(file, contents)) {
			for (unsigned char c : contents) {
				if (c == '\033')
					out += '\033';
				else if (c == 13) {
					out += "\r\n" + std::string(pos.x, ' ');
					for (int i = 0; i < size.y; i++) {
						printf(out.c_str());
						fflush(stdout);
						y++;
					}
					out = "";
				} else
					out += rwm::codepage_437[c];
			}
		}
	}

	void draw_icons() {
		int y = 1;
		int x = (spacing.x - 3) / 2;
		int title_lines = 3;
		DIR* dirp = opendir(desktop_path.c_str());
		dirent* entry;
		desktop_contents.clear();
		while ((entry = readdir(dirp)) != NULL) {
			for (int i = 0; i < 3; i++) {
				int is_dir = (entry->d_type & DT_DIR) != 0;
				wmove(stdscr, y + i, x);
				rwm::waddstr_enc(stdscr, icons[is_dir][i]);
			}
			std::string filename = entry->d_name;
			desktop_contents.push_back(entry->d_name);
			std::string display_name;
			if (filename.length() < title_lines * (spacing.x - 1)) {
				display_name = filename;
			} else {
				display_name = filename.substr(0, title_lines * spacing.x - 6) + "...";
			}
			for (int line = 0; line < title_lines; line++) {
				if (display_name.length() < (line + 1) * (spacing.x - 1)) {
					mvaddstr(y + line + 3, x - (spacing.x - 3) / 2, display_name.substr(
						line * (spacing.x - 1), 
						display_name.length() - line * (spacing.x - 1)
					).c_str());
					break;
				} else {
					mvaddstr(y + line + 3, x - (spacing.x - 3) / 2, display_name.substr(line * (spacing.x - 1), spacing.x - 1).c_str());
				}
			}
			y += spacing.y;

			if (y + spacing.y > stdscr->_maxy) {
				y = 1;
				x += spacing.x;
			}
		}
	}

	void init() {
		//draw_background("./12_DEC23.ANS", {15, 50}, {1, 1});
		draw_icons();
		draw_taskbar();

		return;
	}

	void render() {
		curs_set(0);
		touchwin(stdscr);
		draw_taskbar();
		refresh();
		should_refresh = false;
	} 

	void key_pressed(int key) {
		switch(key) {
			case '\x03':
			rwm::terminate();
			break;
		}
	}

	bool key_priority(int key) {
		if (alt_pressed) {
			if (key == 13) {
				int offset = rwm::windows.size();
				rwm::windows.push_back({{"bash"}, {10 + 5 * offset, 10 + 10 * offset}, {32, 95}, 0});
				should_refresh = true;
				rwm::selected_window = true;
				return true;
			} else if (key == 27) {
				alt_pressed = false;
				return false;
			}
			ungetch(key);
			ungetch(27);
			return true;
		} else {
			switch(key) {
				case 27:
				alt_pressed = true;
				return true;
			}
		}
		return false;
	}


	void click_taskbar(int x) {
		int pos = (x - 4) / (tab_size + 2);
		if (x <= 4) {
			// [rwm] menu
		} else if (pos < rwm::windows.size()){
			if (pos == SEL_WIN && rwm::selected_window) {
				rwm::windows[pos].status |= rwm::HIDDEN;
				rwm::selected_window = false;
				curs_set(0);
			} else {
				rwm::windows[pos].status &= ~rwm::HIDDEN;
				rwm::move_to_top(pos);
				rwm::selected_window = true;
			}
			should_refresh = true;
		}
	}

	void mouse_pressed(MEVENT event) {
		if (event.bstate & BUTTON1_RELEASED) {
			if (event.y == stdscr->_maxy) {
				click_taskbar(event.x);
				click = {-1, -1};
			} else {
				int x, y, y_lim, pos;
				x = event.x / spacing.x;
				y = event.y / spacing.y;
				y_lim = (stdscr->_maxy - 1) / spacing.y;
				pos = y + y_lim * x;
				if (x == click.x && y == click.y) {
					if (pos < desktop_contents.size()) {
						int offset = rwm::windows.size();
						std::string name = desktop_contents[pos];
						rwm::windows.push_back({{"xdg-open", desktop_path + name}, {10 + 5 * offset, 10 + 10 * offset}, {32, 95}, 0});
						rwm::windows[SEL_WIN].title = name;
						rwm::selected_window = true;
					}
					click = {-1, -1};
				} else {
					click = {y, x};
				}
			}
		}
	}

	bool update() {
		return should_refresh;
	}

	bool frame_click(int i, rwm::ivec2 pos, int bstate) {
		rwm::Window& win = rwm::windows[i];
		rwm::ivec2 fpos = {pos.y - win.frame->_begy, pos.x - win.frame->_begx};;
		if (bstate & BUTTON1_PRESSED)  {
			if (fpos.y == 0 && fpos.x >= win.frame->_maxx - 9 && fpos.x < win.frame->_maxx) {
				should_refresh = true;
				switch (fpos.x - win.frame->_maxx) {
				case -8 ... -7:
					// Minimize (TODO)
					win.status |= rwm::HIDDEN;
					if (i == SEL_WIN) {
						rwm::selected_window = false;
						curs_set(0);
					}
					break;
				case -6 ... -4: 
					// Maximize (TODO)
					win.status ^= rwm::MAXIMIZED;
					win.maximize();
				break;

				case -3 ... -2:
					// Close
					rwm::close_window(i);
					break;
				default:
					should_refresh = false;
					break;
				}
				return false;
			}
			drag_pos = pos;
			wattron(win.frame, A_REVERSE);
			box(win.frame, '|', '-');
			wattroff(win.frame, A_REVERSE);
			wrefresh(win.frame);
			return true;
		} else if (bstate & BUTTON1_RELEASED) {
			win.move_by({pos.y - drag_pos.y, pos.x - drag_pos.x});
			draw_taskbar();
			rwm::full_refresh();
			drag_pos = {-1, -1};
			should_refresh = false;
			return false;
		}
		click = {-1, -1};
		return false;
	}

	void frame_render(rwm::Window& win, bool is_focused) {
		if (rwm::utf8) {
			wattron(win.frame, A_REVERSE);
			cchar_t vert_cc;
			cchar_t horiz_cc;
			wchar_t vert  = is_focused ? u'║' : u'│';
			wchar_t horiz = is_focused ? u'═' : u'─';
			setcchar(&vert_cc, &vert, 0, 0, nullptr);
			setcchar(&horiz_cc, &horiz, 0, 0, nullptr);
			box_set(win.frame, &vert_cc, &horiz_cc);
		} else {
			if (is_focused)
				wattron(win.frame, A_REVERSE);
			box(win.frame, ACS_VLINE, ACS_HLINE);
		}
		mvwaddstr(win.frame, 0, 1, win.title.c_str());
		mvwaddstr(win.frame, 0, win.frame->_maxx - 9, "[ - o x ]");
		wattroff(win.frame, A_REVERSE);
		draw_taskbar();
	}
}