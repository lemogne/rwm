#include <ncurses.h>
#include <iostream>
#include <stdio.h>
#include <pty.h>
#include <stdexcept>
#include <unistd.h>
#include <poll.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h> 
#include <sys/wait.h>
#include <vector>
#include <unordered_map>
#include <limits>
#include "windows.hpp"
#include "desktop.hpp"
#include "charencoding.hpp"
#include <cmath>

namespace rwm {
	// Color settings (defaults)
	int base_colors = 16;
	int max_colors = 256;
	int max_color_pairs = 32767;
	const uint64_t DEFAULT_COLOR = -1;
	int color_pairs = 1;
	int colors = 8;
	std::unordered_map<int, short> color_map = {};
	std::unordered_map<uint64_t, chtype> pair_map = {};

	std::vector<Window*> windows = {};
	bool selected_window = false;
	int bold_mode = BOLD;

	char buffer[32768];
	std::ofstream debug_log(getenv("HOME") + std::string("/.rwmlog"), std::ios::app);

	void run_child(std::vector<std::string>& args, int master, int slave) {
		// Source: https://www.rkoucha.fr/tech_corner/pty_pdip.html
		close(master);
		termios term_settings{};
		tcgetattr(slave, &term_settings);
		cfmakeraw(&term_settings);
		tcsetattr(slave, TCSANOW | ECHO | ISIG, &term_settings);

		// The slave side of the PTY becomes the standard input and outputs of the child process
		close(0); // Close standard input (current terminal)
		close(1); // Close standard output (current terminal)
		close(2); // Close standard error (current terminal)

		dup(slave); // PTY becomes standard input (0)
		dup(slave); // PTY becomes standard output (1)
		dup(slave); // PTY becomes standard error (2)

		// Now the original file descriptor is useless
		close(slave);

		// Make the current process a new session leader
		setsid();

		// As the child is a session leader, set the controlling terminal to be the slave side of the PTY
		// (Mandatory for programs like the shell to make them manage correctly their outputs)
		ioctl(0, TIOCSCTTY, 1);
		std::vector<char*> c_args;
		for (std::string const& a : args)
			c_args.emplace_back(const_cast<char*>(a.c_str()));
		c_args.push_back(nullptr);

		// Set environment vars
		if (has_colors() && utf8)
			setenv("TERM", "xterm-color", 1);

		execvp(args[0].c_str(), c_args.data());

		exit(0);
	}

	void Window::launch_program(std::vector<std::string> args) {
		ivec2 size_win = {size.y - 2, size.x - 2};
		winsize wsize;
		if (ioctl(0, TIOCGWINSZ, (char *) &wsize) < 0)
			print_debug("TIOCGWINSZ error");
		wsize.ws_xpixel = (wsize.ws_xpixel / wsize.ws_col) * size_win.x;
		wsize.ws_ypixel = (wsize.ws_ypixel / wsize.ws_row) * size_win.y;
		wsize.ws_row = size_win.y;
		wsize.ws_col = size_win.x;

		if (openpty(&master, &slave, nullptr, nullptr, &wsize)) {
			// For now
			exit(1);
		}

		int flags = fcntl(master, F_GETFL, 0);
		fcntl(master, F_SETFL, flags | O_NONBLOCK);

		pid = fork();
		if (pid == -1) {
			// FAIL; needs to be handled properly
			exit(1);
		} else if (pid == 0) {
			// CHILD
			run_child(args, master, slave);
		}
		close(slave);
		render(false);
	}

	Window::Window(std::vector<std::string> args, ivec2 pos, ivec2 size, int attrib) {
		this->size = size;
		this->pos = pos;
		ivec2 size_win = {size.y - 2, size.x - 2};
		frame = newwin(size.y, size.x, pos.y, pos.x);
		win = derwin(frame, size_win.y, size_win.x, 1, 1);
		scrollok(win, TRUE);
		wtimeout(win, 0);
		idlok(win, TRUE);
		keypad(win, TRUE);

		alt_frame = newwin(size.y, size.x, pos.y, pos.x);
		alt_win = derwin(alt_frame, size_win.y, size_win.x, 1, 1);
		scrollok(alt_win, TRUE);
		wtimeout(alt_win, 0);
		idlok(alt_win, TRUE);
		keypad(alt_win, TRUE);

		title = args[0];
		status = attrib;

		launch_program(args);
	}

	Window::Window(WINDOW* frame, std::string title, int attrib, int master, int slave) {
		getmaxyx(frame, size.y, size.x);
		getbegyx(frame, pos.y, pos.x);
		this->frame = frame;
		ivec2 size_win = {size.y - 2, size.x - 2};
		win = derwin(frame, size_win.y, size_win.x, 1, 1);
		scrollok(win, TRUE);
		wtimeout(win, 0);
		idlok(win, TRUE);
		keypad(win, TRUE);

		alt_frame = newwin(size.y, size.x, pos.y, pos.x);
		alt_win = derwin(alt_frame, size_win.y, size_win.x, 1, 1);
		scrollok(alt_win, TRUE);
		wtimeout(alt_win, 0);
		idlok(alt_win, TRUE);
		keypad(alt_win, TRUE);

		this->title = title;

		this->master = master;
		this->slave = slave;

		status = attrib;
		int x, y;
		getbegyx(this->frame, y, x);
		getmaxyx(this->frame, y, x);
		render(false);
	}

	Window* Window::create_debug() {
		// init debug window
		WINDOW* w = newwin(33, 95, 10, 10);
		if (!w) {
			printf("Could not create debug window!\n");
			exit(1);
		}
		int x, y;
		getbegyx(w, y, x);
		getmaxyx(w, y, x);
	
		int master, slave;
		ivec2 size_win = {31, 93};

		winsize wsize;
		if (ioctl(0, TIOCGWINSZ, (char *) &wsize) < 0)
			printf("TIOCGWINSZ error");
		wsize.ws_xpixel = (wsize.ws_xpixel / wsize.ws_col) * size_win.x;
		wsize.ws_ypixel = (wsize.ws_ypixel / wsize.ws_row) * size_win.y;
		wsize.ws_row = size_win.y;
		wsize.ws_col = size_win.x;

		if (openpty(&master, &slave, nullptr, nullptr, &wsize)) {
			// For now
			exit(1);
		}

		int flags = fcntl(master, F_GETFL, 0);
		fcntl(master, F_SETFL, flags | O_NONBLOCK);

		termios term_settings{};
		tcgetattr(slave, &term_settings);
		cfmakeraw(&term_settings);
		tcsetattr(slave, TCSANOW | ECHO | ISIG, &term_settings);

		close(2);
		int a = dup(slave);
		close(slave);
		slave = 2;

		setsid();
		ioctl(2, TIOCSCTTY, 1);
		
		return new Window(w, "DEBUG WINDOW", 0, master, slave);
	}

	void Window::render(bool is_focused) {
		if (!(status & HIDDEN)) {
			if (!(status & rwm::FULLSCREEN))
				rwm_desktop::frame_render(*this, is_focused);
			curs_set(state.cursor);
			wnoutrefresh(frame);
			wnoutrefresh(win);
			should_refresh = false;
		}
	}

	int Window::destroy() {
		if (!(status & ZOMBIE))
			kill(pid, SIGHUP);
		int exit_status;
		pid_t retval = waitpid(pid, &exit_status, WNOHANG);
		if (!retval)
			status |= ZOMBIE | SHOULD_CLOSE;
		else { 
			delwin(win);
			delwin(frame);
			delwin(alt_win);
			delwin(alt_frame);
			close(master);
		}
		return retval;
	}

	void Window::maximize() {
		if (status & rwm::FULLSCREEN) {
			box(frame, ' ', ' ');
			mvwin(frame, 0, 0);
			mvwin(alt_frame, 0, 0);
			mvwin(win, 0, 0);
			mvwin(alt_win, 0, 0);
			wresize(frame, getmaxy(stdscr), getmaxx(stdscr));
			wresize(alt_frame, getmaxy(stdscr), getmaxx(stdscr));
			wresize(win, getmaxy(stdscr) - 1, getmaxx(stdscr) - 1);
			wresize(alt_win, getmaxy(stdscr) - 1, getmaxx(stdscr) - 1);

		} else if (status & rwm::MAXIMIZED) {
			box(frame, ' ', ' ');
			mvwin(frame, 0, 0);
			mvwin(alt_frame, 0, 0);
			mvwin(win, 1, 1);
			mvwin(alt_win, 1, 1);
			wresize(frame, getmaxy(stdscr) - 1, getmaxx(stdscr));
			wresize(alt_frame, getmaxy(stdscr) - 1, getmaxx(stdscr));
			wresize(win, getmaxy(stdscr) - 3, getmaxx(stdscr) - 2);
			wresize(alt_win, getmaxy(stdscr) - 3, getmaxx(stdscr) - 2);

		} else {
			box(frame, ' ', ' ');
			wresize(frame, size.y, size.x);
			wresize(alt_frame, size.y, size.x);
			wresize(win, size.y - 2, size.x - 2);
			wresize(alt_win, size.y - 2, size.x - 2);
			mvwin(frame, pos.y, pos.x);
			mvwin(alt_frame, pos.y, pos.x);
			mvwin(win, pos.y + 1, pos.x + 1);
			mvwin(alt_win, pos.y + 1, pos.x + 1);
		}
		winsize wsize;
		rwm::ivec2 size_win = {getmaxy(win), getmaxx(win)};
		if (ioctl(0, TIOCGWINSZ, (char *) &wsize) < 0)
			print_debug("TIOCGWINSZ error");
		wsize.ws_xpixel = (wsize.ws_xpixel / wsize.ws_col) * size_win.x;
		wsize.ws_ypixel = (wsize.ws_ypixel / wsize.ws_row) * size_win.y;
		wsize.ws_row = size_win.y;
		wsize.ws_col = size_win.x;
		ioctl(master, TIOCSWINSZ, (char *) &wsize);
		should_refresh = true;
	}

	void Window::resize(ivec2 size) {
		if (status & (rwm::MAXIMIZED | rwm::FULLSCREEN)) 
			return;

		this->size = size;
		box(frame, ' ', ' ');
		wresize(frame, size.y, size.x);
		wresize(alt_frame, size.y, size.x);
		wresize(win, size.y - 2, size.x - 2);
		wresize(alt_win, size.y - 2, size.x - 2);
		winsize wsize;
		rwm::ivec2 size_win = {getmaxy(win), getmaxx(win)};
		if (ioctl(0, TIOCGWINSZ, (char *) &wsize) < 0)
			print_debug("TIOCGWINSZ error");
		wsize.ws_xpixel = (wsize.ws_xpixel / wsize.ws_col) * size_win.x;
		wsize.ws_ypixel = (wsize.ws_ypixel / wsize.ws_row) * size_win.y;
		wsize.ws_row = size_win.y;
		wsize.ws_col = size_win.x;
		ioctl(master, TIOCSWINSZ, (char *) &wsize);
		should_refresh = true;
	}


	std::unordered_map<int, chtype> attr_modes_on = {
		{1, A_BOLD},
		{2, A_DIM},
		{3, A_ITALIC},
		{4, A_UNDERLINE},
		{5, A_BLINK},
		{7, A_REVERSE},
		{11, A_ALTCHARSET},
		{12, A_ALTCHARSET},
		{21, A_UNDERLINE}
	};
	std::unordered_map<int, chtype> attr_modes_off = {
		{22, A_DIM},
		{23, A_ITALIC},
		{24, A_UNDERLINE},
		{25, A_BLINK},
		{27, A_REVERSE},
		{10, A_ALTCHARSET}
	};

	// COLOR
	int get_rgb(int c) {
		int red = (c >> 16) & 0xff;
		int green = (c >> 8) & 0xff;
		int blue = c & 0xff;
		if ((c >> 24) == 1 && blue >= 16) {
			if (blue < 232) {
				blue -= 16;
				red = blue / 36;
				green = blue / 6 - 6 * red;
				blue %= 6;
				red *= 51;
				green *= 51;
				blue *= 51;
			} else {
				blue -= 231;
				blue *= 255;
				blue /= 25;
				red = green = blue;
			}
		} else if ((c >> 24) == 2) {
			return c;
		} else {
			int bright = (blue & 8) ? 128 : 0;
			red = (blue & 1) ? 255 : bright;
			green = (blue & 2) ? 255 : bright;
			blue = (blue & 4) ? 255 : bright;
		}
		return blue | (green << 8) | (red << 16) | (2 << 24);
	}

	int find_closest_color(int r, int g, int b) {
		int closest = 7;
		double closest_d = std::numeric_limits<double>::infinity();
		for (auto i : color_map) {
			int c = get_rgb(i.first);
			int red = (c >> 16) & 0xff;
			int green = (c >> 8) & 0xff;
			int blue = c & 0xff;
			double d = sqrt((red - r) * (red - r) +  (green - g) * (green - g) + (blue - b) * (blue - b));
			if (d < closest_d) {
				closest_d = d;
				closest = i.first;
			}
		}
		return closest;
	}

	uint64_t find_closest_pair(int c1, int c2) {
		uint64_t closest = 7;
		double closest_d = std::numeric_limits<double>::infinity();
		for (auto i : pair_map) {
			uint64_t p = i.first;
			int p1 = p >> 32;
			int p2 = p;
			int p2_r = (get_rgb(p2) >> 16) & 0xff;
			int p2_g = (get_rgb(p2) >> 8)  & 0xff;
			int p2_b =  get_rgb(p2)        & 0xff;

			int p1_r = (get_rgb(p1) >> 16) & 0xff;
			int p1_g = (get_rgb(p1) >> 8)  & 0xff;
			int p1_b =  get_rgb(p1)        & 0xff;

			int c2_r = (get_rgb(c2) >> 16) & 0xff;
			int c2_g = (get_rgb(c2) >> 8)  & 0xff;
			int c2_b =  get_rgb(c2)        & 0xff;

			int c1_r = (get_rgb(c1) >> 16) & 0xff;
			int c1_g = (get_rgb(c1) >> 8)  & 0xff;
			int c1_b =  get_rgb(c1)        & 0xff;

			double d = sqrt((p2_r - c2_r) * (p2_r - c2_r) + (p2_g - c2_g) * (p2_g - c2_g) + (p2_b - c2_b) * (p2_b - c2_b) +
			        (p1_r - c1_r) * (p1_r - c1_r) + (p1_g - c1_g) * (p1_g - c1_g) + (p1_b - c1_b) * (p1_b - c1_b));

			if (d < closest_d) {
				closest_d = d;
				closest = i.first;
			}
		}
		return closest;
	}

	void Window::apply_color(int c, bool bg) {
		auto color_it = color_map.find(c);
		if (color_it == color_map.end()) {
			int red = (c >> 16) & 0xff;
			int green = (c >> 8) & 0xff;
			int blue = c & 0xff;
			if ((c >> 24) == 1) {
				if (blue < 16) {
					c = blue;
					goto found;
				} else if (blue < 232) {
					blue -= 16;
					red = blue / 36;
					green = blue / 6 - 6 * red;
					blue %= 6;
					red *= 51;
					green *= 51;
					blue *= 51;
				} else {
					blue -= 231;
					blue *= 255;
					blue /= 25;
					red = green = blue;
				}
			}
			if (can_change_color() && colors < max_colors) {
				if (HAS_EXT_COLOR)
					init_extended_color(colors, red * 1000 / 255, green * 1000 / 255, blue * 1000 / 255);
				else
					init_color(colors, red * 1000 / 255, green * 1000 / 255, blue * 1000 / 255);
				color_map.insert_or_assign(c, colors);
				colors++;
			} else {
				c = find_closest_color(red, green, blue);
			}
		}
	found:
		if (base_colors < 16 && c < 16) {
			if (c >= base_colors) {
				if ((bg && bold_mode == BRIGHT_BG) || (!bg && bold_mode == BRIGHT_FG)) {
					state.attrib |= A_BOLD;
				} else if ((bg && bold_mode == DARK_BG) || (!bg && bold_mode == DARK_FG)) {
					state.attrib &= ~A_BOLD;
				}
				c = c - base_colors;
			} else if ((bg && bold_mode == DARK_BG) || (!bg && bold_mode == DARK_FG)) {
				state.attrib |= A_BOLD;
			} else if ((bg && bold_mode == BRIGHT_BG) || (!bg && bold_mode == BRIGHT_FG)) {
				state.attrib &= ~A_BOLD;
			} 
		}

		if (bg) {
			state.color = (((uint64_t) c) << 32) | (state.color & 0xffffffff);
		} else {
			state.color = (state.color & 0xffffffff00000000) | c;
		}
	}

	void set_color_rgb(WINDOW* win, char red_fg, char green_fg, char blue_fg, char red_bg, char green_bg, char blue_bg) {

	}

	int create_vga_color(int color) {
		int red, green, blue;
		blue = color;
		if (color < 16) {
			return color;
		} else if (color < 232) {
			blue -= 16;
			red = blue / 36;
			green = blue / 6 - 6 * red;
			blue %= 6;
			red *= 51;
			green *= 51;
			blue *= 51;
		} else {
			blue -= 231;
			blue *= 255;
			blue /= 25;
			red = green = blue;
		}
		if (can_change_color() && colors < max_colors) {
			if (HAS_EXT_COLOR)
				init_extended_color(colors, red * 1000 / 255, green * 1000 / 255, blue * 1000 / 255);
			else
				init_color(colors, red * 1000 / 255, green * 1000 / 255, blue * 1000 / 255);
			color_map.insert_or_assign(color, colors);
			return colors++;
		} else {
			return color_map.at(find_closest_color(red, green, blue));
		}
	}

	void set_color_vga(WINDOW* win, int color_fg, int color_bg) {
		int bg = color_bg;
		int fg = color_fg;
		bg |= (bg >= 16) ? (1 << 24) : 0;
		fg |= (fg >= 16) ? (1 << 24) : 0;
		uint64_t color = ((uint64_t) bg << 32) | fg;
		auto pair = pair_map.find(color);
		int ipair;
		if (pair != pair_map.end()) {
			ipair = pair->second;
		} else {
			// Factor out into function
			int fg_no, bg_no;
			auto fg_no_p = color_map.find(fg);
			auto bg_no_p = color_map.find(bg);
			if (fg_no_p != color_map.end())
				fg_no = fg_no_p->second;
			else {
				fg_no = create_vga_color(color_fg);
			}
			if (bg_no_p != color_map.end())
				bg_no = bg_no_p->second;
			else {
				bg_no = create_vga_color(color_bg);
			}
			if (color_pairs < max_color_pairs) {
				if (HAS_EXT_COLOR) 
					init_extended_pair(color_pairs, fg_no, bg_no);
				else 
					init_pair(color_pairs, fg_no, bg_no);
				pair_map.insert_or_assign(color, color_pairs);
				ipair = color_pairs;
				color_pairs++;
			} else {
				color = find_closest_pair(bg_no, fg_no);
				ipair = pair_map.at(color);
			}
		}
		if (HAS_EXT_COLOR)
			wcolor_set(win, ipair, nullptr);
		else
			wattron(win, COLOR_PAIR(ipair));
	}

	void Window::apply_color_pair() {
		int fc = state.color & 0xffffffff;
		int bc = state.color >> 32;
		uint64_t pair = state.color;

		auto pair_it = pair_map.find(pair);
		if (pair_it == pair_map.end()) {
			if (color_pairs < max_color_pairs) {
				if (HAS_EXT_COLOR) 
					init_extended_pair(color_pairs, color_map.at(fc), color_map.at(bc));
				else 
					init_pair(color_pairs, color_map.at(fc), color_map.at(bc));
				pair_map.insert_or_assign(pair, color_pairs);
				color_pairs++;
			} else {
				pair = find_closest_pair(bc, fc);
			}
		}
		state.color_pair = pair_map.at(pair);
	}

	// WINDOW ATTRIBUTES

	void Window::set_attrib() {
		int custom_color_mode = 0;
		int col = 0;
		if (state.ctrl.size() == 0){
			state.attrib = 0;
			state.color = DEFAULT_COLOR;
		}
		for (int c : state.ctrl) {
			if (custom_color_mode) {
				switch (custom_color_mode % 10) {
				case 0:
				custom_color_mode += c;
				continue;

				case 2 ... 3:
				col <<= 8;
				col |= c;
				custom_color_mode++;
				continue;

				case 4:
				col <<= 8;
				col |= c;
				apply_color(col | 0x02000000, custom_color_mode / 40);
				custom_color_mode = 0;
				continue;

				case 5:
				apply_color(c | 0x01000000, custom_color_mode / 40);
				custom_color_mode = 0;
				continue;

				default:
				if (DEBUG)
					print_debug("error: attribute " + std::to_string(c));
				continue;

				}
			}
			if (c == 0) {
				state.attrib = 0;
				state.color = DEFAULT_COLOR;
			}

			// Turn on attributes
			auto mode = attr_modes_on.find(c);
			if (mode != attr_modes_on.end()) {
				state.attrib |= mode->second;
			}

			// Turn off attributes
			mode = attr_modes_off.find(c);
			if (mode != attr_modes_off.end()) {
				state.attrib &= ~(mode->second);
			}

			switch (c) {
			case 30 ... 37:
			case 40 ... 47:
			case 90 ... 97:
			case 100 ... 107:
			apply_color((c % 10) + ((c >= 90) ? 8 : 0), (c % 20) < 10);
			break;

			case 38: case 48:
			custom_color_mode = c - 8;
			break;

			case 39:
			apply_color((uint32_t) DEFAULT_COLOR, false);
			break;

			case 49:
			apply_color((uint32_t) (DEFAULT_COLOR >> 32), true);
			break;
			}
		}
		if (HAS_EXT_COLOR)
			wattrset(win, state.attrib | COLOR_PAIR(state.color_pair));
		else
			wattr_set(win, state.attrib, state.color_pair, nullptr);
	}

	void Window::do_osc() {
		if (state.ctrl[0] == 0 || state.ctrl[0] == 2) {
			title = state.out;
		} else if (state.ctrl[0] == 112) {
			state.color = DEFAULT_COLOR;
		}
	}

	void Window::do_dcs() {
		//char error_msg[] = "\033P0$r\033\\";
		//write(windows[SEL_WIN]->master, error_msg, sizeof(error_msg) - 1);
	}	

	void Window::do_private_seq(char mode) {
		int n1;
		n1 = (state.ctrl.size() > 0) ? state.ctrl[0] : 0;
		switch(n1) {
			case 1:
			status = (status | ((mode == 'h') ? APP_CURSOR : 0)) & ~((mode == 'l') ? APP_CURSOR : 0);
			break;

			case 25:
			state.cursor = (mode == 'h') ? 1 : (mode == 'l') ? 0 : state.cursor;
			break;

			case 1000: case 1005: case 1006: case 1015: case 1016:
			mouse_mode = (mode == 'h') ? n1 : (mode == 'l') ? 0 : mouse_mode;
			break;

			case 1004:
			status = (status | ((mode == 'h') ? REPORT_FOCUS : 0)) & ~((mode == 'l') ? REPORT_FOCUS : 0);
			break;

			// TODO:
			case 1048: 
			if (mode == 'h') {
				int x, y;
				getyx(win, y, x);
				state.saved_cursor_pos = {y, x};
			} else if (mode == 'l') {
				wmove(win, state.saved_cursor_pos.y, state.saved_cursor_pos.x);
			}
			break;

			case 47:
			case 1049:
			case 1047:
			if ((mode == 'h' && alt_win_no == 0) || (mode == 'l' && alt_win_no == 1)) {
				WINDOW* frame_temp = frame;
				WINDOW* win_temp = win;
				frame = alt_frame;
				win = alt_win;
				alt_frame = frame_temp;
				alt_win = win_temp;
				alt_win_no ^= 1;
			}
			break;

			default:
			if (DEBUG)
				print_debug(state.esc_seq);
			break;

			//case 2004:		// TODO

		}
	}

	void Window::move_cursor(char mode) {
		int x, y, n1, n2;
		getyx(win, y, x);
		n1 = (state.ctrl.size() > 0) ? std::max(state.ctrl[0], 1) : 1;
		n2 = (state.ctrl.size() > 1) ? std::max(state.ctrl[1], 1) : 1;
		//this->should_refresh = true;
		switch (mode) {
			case 'A':
			wmove(win, y - n1, x);
			break;
			case 'B': case 'e':
			wmove(win, y + n1, x);
			break;
			case 'C': case 'a':
			wmove(win, y, x + n1);
			break;
			case 'D':
			wmove(win, y, x - n1);
			break;
			case 'E':
			wmove(win, y + n1, 0);
			break;
			case 'F':
			wmove(win, y - n1, 0);
			break;
			case 'G': case '`':
			wmove(win, y, n1 - 1);
			break;
			case 'H': case 'f':
			wmove(win, n1 - 1, n2 - 1);
			break;
			case 'd':
			wmove(win, n1 - 1, x);
			break;

			case 'S':
			wscrl(win, n1);
			break;
			case 'T':
			wscrl(win, -n1);
			break;

			case 's':
			state.saved_cursor_pos = {y, x};
			break;
			case 'u':
			wmove(win, state.saved_cursor_pos.y, state.saved_cursor_pos.x);
			break;

			case 'n':
			if (n1 == 6) {
				std::string response = "\033[" + std::to_string(y + 1) + ';' + std::to_string(x + 1) + 'R';
				send(response);
				if (DEBUG)
					print_debug(response);
			}
			break;

			default:
				break;
		}
	}

	void Window::flatten_buffers() {
		delwin(alt_win);
		delwin(alt_frame);
		alt_win = win;
		alt_frame = frame;
	}

	void Window::move_by(ivec2 d) {
		move({getbegy(frame) + d.y, getbegx(frame) + d.x});
	}

	void Window::move(ivec2 pos) {
		if (status & (rwm::MAXIMIZED | rwm::FULLSCREEN))
			return;

		ivec2 offset = {getbegy(win) - getbegy(frame), getbegx(win) - getbegx(frame)};
		int can_move = mvwin(frame, pos.y, pos.x);
		if (can_move == ERR)
			return;
		this->pos = pos;
		mvwin(alt_frame, pos.y, pos.x);

		mvwin(win, pos.y + offset.y, pos.x + offset.x);
		mvwin(alt_win, pos.y + offset.y, pos.x + offset.x);
	}

	void Window::erase(char mode) {
		int n1 = (state.ctrl.size() > 0) ? state.ctrl[0] : 0;
		switch (mode) {
			case 'J':
			switch(n1) {
				case 0:
				wclrtobot(win);
				break;
				case 2:
				wclear(win);
				break;
			}
			break;

			case 'K':
			switch(n1) {
				case 0:
				wclrtoeol(win);
				break;
				case 2: {
					int x, y;
					getyx(win, y, x);
					wmove(win, y, 0);
					wclrtoeol(win);
					wmove(win, y, x);
				}
				break;
			}
			break;


			// hacky solution to respect scrolling regions, which ncurses refuses to do
			case 'L':{
				flush();
				int x, y, top, bot;
				getyx(win, y, x);
				wgetscrreg(win, &top, &bot);
				wsetscrreg(win, y, bot);
				wscrl(win, -std::max(n1, 1));
				wmove(win, y, 0);
				wsetscrreg(win, top, bot);
			}
			break;

			case 'M': {
				flush();
				int x, y, top, bot;
				getyx(win, y, x);
				wgetscrreg(win, &top, &bot);
				wsetscrreg(win, y, bot);
				wscrl(win, std::max(n1, 1));
				wmove(win, y, 0);
				wsetscrreg(win, top, bot);
			}
			break;

			case 'P':
			flush();
			for (int i = 0; i < n1; i++)
				wdelch(win);
			break;

			case 'X': {
				int x, y;
				getyx(win, y, x);
				state.out += std::string(n1, ' ');
				flush();
				wmove(win, y, x);
			}
			break;
		}
	}

	void Window::flush() {
		int x, y;
		getyx(win, y, x);
		apply_color_pair();
		if (HAS_EXT_COLOR)
			wattr_set(win, state.attrib, state.color_pair, nullptr);
		else
			wattrset(win, state.attrib | COLOR_PAIR(state.color_pair));
		if (status & INSERT) {
			winsstr(win, state.out.c_str());
			wmove(win, y, x + utf8length(state.out));
			return;
		}

		// Complicated scrolling code to override default ncurses scrolling behaviour
		scrollok(win, FALSE);
		int maxlen;
		if (state.auto_nl)
			maxlen = getmaxx(win) - x + ((getmaxy(win) - y - 1) * (getmaxx(win) - 1));
		else
			maxlen = getmaxx(win) - x + 1;
		waddstr_enc(win, utf8substr(state.out, 0, maxlen));
		if (DEBUG)
			debug_log << utf8substr(state.out, 0, maxlen) << '\n';

		if (state.auto_nl) {
			for (int i = maxlen; i < utf8length(state.out); i += getmaxx(win) - 1) {
				scrollok(win, TRUE);
				scroll(win);
				wmove(win, y, 0);
				scrollok(win, FALSE);
				waddstr_enc(win, utf8substr(state.out, i, i + getmaxx(win) - 2));
				if (DEBUG)
					debug_log << utf8substr(state.out, i, i + getmaxx(win) - 2) << '\n';
			}
		} else if (maxlen <= utf8length(state.out)) {
			wmove(win, y, getmaxx(win) - 1);
		}

		scrollok(win, TRUE);
		state.out = "";
	}

	void print_debug(std::string msg) {
		static int x = 0;
		static int y = -1;
		y++;
		if (y >= 30) {
			x += 15;
			y = 0;
		}
		if (x >= 85) {
			x = 0;
			fprintf(stderr, "\033[2J");
		}
		fprintf(stderr, "\033[%d;%dH%s\n", y+1, x+1, msg.c_str());
		debug_log << msg << '\n';
	}

	void Window::send(std::string message) {
		if (!(status & ZOMBIE))
			write(master, message.c_str(), message.size());
	}

	void Window::send(char c) {
		if (!(status & ZOMBIE))
			write(master, &c, 1);
	}

	int Window::output() {
		int ret = sizeof buffer;
		int should_refresh = this->should_refresh;
		this->should_refresh = false;
		if (status & ZOMBIE)
			return 1;
		for (int i = 0; i < ret; i++) {
			if (i == 0) {
				pollfd p{master, POLLHUP, 1};
				poll(&p, 1, 1);
				if ((p.revents & (POLLHUP | POLLERR | POLLNVAL | POLLRDHUP)))
					status |= SHOULD_CLOSE;

				ret = read(master, buffer, sizeof buffer);
				if (ret <= 0) {
					if (state.is_text && state.out != "") {
						flush();
						return 1;
					}
					return should_refresh; // No data or closed
				}
			}
			if (state.is_text) {
				state.esc_seq = "";
				if (buffer[i] < 32 && DEBUG && slave != 2)
					print_debug(((buffer[i] != 10 && buffer[i] != 13) ? std::string(1, buffer[i]) : "\\n") + ' ' + std::to_string((int) buffer[i]));
				switch (buffer[i]) {
					case '\x1B':
					if (state.out != "") {
						flush();
						should_refresh = 1;
					}
					state.is_text = false;
					state.ctrl.clear();
					state.esc_type = buffer[i];
					continue;

					case '\r': {
						if (state.out != "") {
							flush();
							should_refresh = 1;
						}
						int x, y;
						getyx(win, y, x);
						wmove(win, y, 0);
					}
					continue;

					case '\n': {
						if (state.out != "") {
							flush();
							should_refresh = 1;
						}
						int x, y, top, bot;
						getyx(win, y, x);
						wgetscrreg(win, &top, &bot);
						if (y >= bot) {
							scroll(win);
							wmove(win, y, x);
						} else {
							wmove(win, y + 1, x);
						}
						flush();
					}
					continue;

					case 14:
					state.vt220 = true;
					continue;

					case '\a':
					beep();
					continue;

					case '\x0F':
					state.vt220 = false;
					continue;

					case '\b':
					flush();
					if (status & INSERT)
						winsch(win, '\b');
					else
						waddch(win, '\b');
					should_refresh = 1;
					continue;

					default:
					break;
				}
				if (state.vt220) {
					flush();
					waddch(win, NCURSES_ACS(buffer[i]));
				} else
					state.out += buffer[i];
			} else {
				state.esc_seq += buffer[i];
				if (state.esc_type == '\xFF') {
					if (buffer[i] == '\7' || buffer[i] == '\x9C' || buffer[i] == '\\') {
						if (buffer[i] != '\\' || state.esc_seq[state.esc_seq.length() - 2] == '\033') {
							do_osc();
							should_refresh = 1;
							state.is_text = true;
							state.out = "";
						} else
							state.out += '\033';
					} else if (buffer[i] != '\033')
						state.out += buffer[i];
					continue;
				} else if (state.esc_type == '\x90') {
					if (buffer[i] == '\\' || buffer[i] == '\7') {
						do_dcs();
						if (DEBUG)
							print_debug(state.esc_seq);
						should_refresh = 1;
						state.is_text = true;
						state.out = "";
					}
					continue;
				}
				switch (buffer[i]) {
				case 'A' ... 'H': case 'S' ... 'T': case 'd' ... 'f':
				case 's': case 'u': case 'n': case 'W': case 'Z': case '`' ... 'a':
				if (state.esc_type == '[')
					move_cursor(buffer[i]);
				else if (state.esc_type == '(' && buffer[i] == 'B')
					state.vt220 = false;
				else if (DEBUG) {
					print_debug(state.esc_seq);
					should_refresh = 1;
				}
				break;

				case 'J' ... 'M': case 'P': case 'X':
				if (state.esc_type == '[') {
					erase(buffer[i]);
					should_refresh = 1;
				} else if (state.esc_type == '\x1B' && buffer[i] == 'M') {
					int x, y, top, bot;
					getyx(win, y, x);
					wgetscrreg(win, &top, &bot);
					if (y <= top) {
						wscrl(win, -1);
						wmove(win, y - 1, x);
					} else {
						wmove(win, y - 1, x);
					}
				} else if (buffer[i] == 'P') {
					state.esc_type = '\x90';
					continue;
				} else if (DEBUG) {
					print_debug(state.esc_seq);
					should_refresh = 1;
				}
				break;

				case 'h': case 'l': {
					int n1 = (state.ctrl.size() > 0) ? std::max(state.ctrl[0], 0) : 0;
					if (state.esc_type == '[' && state.out == "?")
						do_private_seq(buffer[i]);
					else if (state.esc_type == '[' && n1 == 4)
						status = (buffer[i] == 'h') ? (status | INSERT) : (status & ~INSERT);
					else if (state.esc_type == '[' && n1 == 20)
						state.auto_nl = (buffer[i] == 'h');
					else if (DEBUG) {
						print_debug(state.esc_seq);
						should_refresh = 1;
					}
				}
				break;

				case 'r':
					if (state.esc_type == '[') {
						int n1 = (state.ctrl.size() > 0) ? std::max(state.ctrl[0], 0) : 0;
						int n2 = (state.ctrl.size() > 1) ? std::max(state.ctrl[1], 0) : 0;
						int margins[2];
						margins[0] = (n1 == 0) ? 0 : n1 - 1;
						margins[1] = (n2 == 0) ? getmaxy(win) - 1 : n2 - 1;
						wsetscrreg(win, margins[0], margins[1]);
					} else if (DEBUG) {
						print_debug(state.esc_seq);
						should_refresh = 1;
					}
				break;
				
				case 'm':
				if (state.esc_type == '[' && state.esc_seq[1] != '?')
					set_attrib();
				else if (DEBUG) {
					print_debug(state.esc_seq);
					should_refresh = 1;
				}
				break;

				case '\x1B':
				state.ctrl.clear();
				state.esc_type = '\x1B';
				state.out = "";
				continue;

				default:
				if (DEBUG) {
					print_debug(state.esc_seq);
					should_refresh = 1;
				}
				break;

				case ';': case ':':
				if (state.esc_type == ']')
					state.esc_type = '\xFF';
				else
					state.ctrl.push_back(0);
				continue;

				case '?': case '(': case ')':
				case '[' ... '_': case '>': case '$': case ' ':
				if (state.ctrl.size() < 1 && state.esc_type == '\x1B')
					state.esc_type = buffer[i];
				else if (state.esc_type == '[' && buffer[i] == '^')
					move_cursor(buffer[i]);
				else
					state.out += buffer[i];
				continue;

				case '0' ... '9':
				if (state.esc_type == '(' && buffer[i] == '0') {
					state.vt220 = true;
					break;
				} else if (state.esc_type == '\x1B') {
					if (DEBUG) {
						print_debug(state.esc_seq);
						should_refresh = 1;
					}
					break;
				} else if (state.esc_type != '[' && state.esc_type != ']') {
					if (DEBUG) {
						print_debug(state.esc_seq);
						should_refresh = 1;
					}
					break;
				}
				if (state.ctrl.size() < 1)
					state.ctrl.push_back(0);
				state.ctrl[state.ctrl.size() - 1] *= 10;
				state.ctrl[state.ctrl.size() - 1] += buffer[i] - '0';
				continue;
				}
				state.is_text = true;
				if (DEBUG && slave != 2)
					print_debug(state.esc_seq);
				state.out = "";
				continue;
			}
		}
		return should_refresh;
	}
}
