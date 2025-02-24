#include <termios.h>
#include <fcntl.h>
#include <pty.h>
#include <iomanip>
#include <chrono>
#include <sstream>
#include <fstream>
#include <dirent.h>
#include <unistd.h>
#include <algorithm>
#include <sys/stat.h>
#include <iostream>
#include "rwm.h"
#include "rwmdesktop.hpp"
#include "charencoding.hpp"
#include "settings.cpp"

namespace rwm_desktop {
	const std::string version = "0.9";

	int resize_mode = OFF;
	bool should_refresh = false;
	bool alt_pressed = false;
	int tiled_mode = 0;
	bool vertical_mode = false;
	bool should_draw_icons = true;
	std::string desktop_path = getenv("HOME") + std::string("/Desktop/");
	std::string cwd = getenv("HOME");
	char fifo_path[] = "/tmp/rwm/open.fifo";
	int fifofd = -1;
	rwm::Window* background;
	std::vector<std::string> background_program = {};
	std::vector<std::string> desktop_contents = {};
	int tab_size = 20;
	rwm::ivec2 spacing = {6, 10};
	rwm::ivec2 win_size = {32, 95};
	rwm::ivec2 drag_pos = {-1, -1};
	rwm::ivec2 click = {-1, -1};
	std::string shell = "bash";
	std::string rwm_config = "";
	std::string rwm_bin = "";

	// Theme 
	int theme[2] = {-1, 12};
	int frame_theme[2] = {-1, 12};
	std::string frame_chars[32] = {
		"│", "│", "─", "─", "┌", "┐", "└", "┘",
		"║", "║", "═", "═", "╔", "╗", "╚", "╝",
		"|", "|", "-", "-", "┌", "┐", "└", "┘",
		"*", "*", "*", "*", "*", "*", "*", "*"
	};
	char ascii_frame_chars[32] = {
		ACS_VLINE, ACS_VLINE, ACS_HLINE, ACS_HLINE, ACS_ULCORNER, ACS_URCORNER, ACS_LLCORNER, ACS_LRCORNER,
		ACS_VLINE, ACS_VLINE, ACS_HLINE, ACS_HLINE, ACS_ULCORNER, ACS_URCORNER, ACS_LLCORNER, ACS_LRCORNER,
		'|', '|', '-', '-', ACS_ULCORNER, ACS_URCORNER, ACS_LLCORNER, ACS_LRCORNER,
		'*', '*', '*', '*', '*', '*', '*', '*'
	};
	std::string buttons[3] = {"[rwm]", "[ - o x ]", "[]"};

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

	char icon_colors[2][2] = {
		{-1, 7},
		{-1, 11}
	};

	void set_selected(rwm::Window* win) {
		for (int i = 0; i < rwm::windows.size(); i++) {
			if (rwm::windows[i] == win) {
				if (tiled_mode == TABBED) {
					if (P_SEL_WIN) 
						P_SEL_WIN->status &= ~rwm::MAXIMIZED;
					win->status |= rwm::MAXIMIZED;
				}
				rwm::set_selected(i);
				should_refresh = true;
				break;
			}
		}
	}

	struct cell {
		struct iterator {
			friend class cell;
			public:
			rwm::Window* operator *() const { 
				if (owner.window == nullptr) {
					if (index < owner.cells.size())
						return **it;
					else
						return nullptr;
				} else {
					return (index == 0) ? owner.window : nullptr;
				}
			}
			const iterator &operator ++() {
				if (owner.window != nullptr) {
					index++;
				} else {
					++*it;
					if (**it == nullptr) {
						index++;
						delete it;
						if (index < owner.cells.size())
							it = new iterator(*owner.cells[index], 0);
						else
							it = nullptr;
					}
				}
				return *this;
			}

			bool operator ==(const iterator &other) const {return **this == *other;}
			bool operator !=(const iterator &other) const {return **this != *other;}

			//protected:
			iterator(cell& owner, int index) : owner (owner), index(index) {
				if (owner.window == nullptr && owner.cells.size() > 0 && index < owner.cells.size())
					it = new iterator(*owner.cells[index], 0);
				else
					it = nullptr;
			}

			~iterator() {if (it) delete it;}

			private:
			int index;
			iterator* it;
			cell& owner;
		};
		rwm::Window* window;
		std::vector<cell*> cells{};
		bool vertical;
		cell* parent;

		iterator begin() {return iterator(*this, 0);}

		iterator end() {return iterator(*this, (this->window == nullptr) ? cells.size() : 1);}

		struct cell_index {
			cell* c;
			std::vector<int> indices;
		};

		void add(rwm::Window* win, cell_index& j) {
			if (window != nullptr) {
				vertical = vertical_mode;
				cells.push_back(new cell{window, {}, !vertical, this});
				window = nullptr;
			} else if (cells.size() == 0) {
				vertical = vertical_mode;
				window = win;
				return;
			} else if (cells.size() == 1) {
				vertical = vertical_mode;
			} else if (vertical != vertical_mode) {
				j.c->cells[j.indices.back()]->add(win, j);
				return;
			}
			if (j.indices.size() > 0)
				cells.insert(cells.begin() + j.indices.back(), new cell{win, {}, !vertical, this});
			else
				cells.push_back(new cell{win, {}, !vertical, this});
		}

		int remove(rwm::Window* win) {
			if (window == win) {
				window = nullptr;
				return -1;
			}
			
			for (int i = 0; i < cells.size(); i++) {
				switch (cells[i]->remove(win)) {
					case -1:
					delete cells[i];
					cells.erase(cells.begin() + i);
					if (cells.size() == 0)
						return -1;
					else if (cells.size() == 1) 
						return 2;
					//fallthrough;
					case 1:
					return 1;

					case 2:
					cells[i] = cells[i]->cells[0];
					cells[i]->parent = this;
					return 1;

					default: 
					continue;
				}
			}
			return 0;
		}

		rwm::Window* get(int& i) {
			if (window == nullptr) {
				for (int j = 0; j < cells.size(); j++) {
					rwm::Window* ccell = cells[j]->get(i);
					if (ccell != nullptr)
						return ccell;
				}
			} else {
				if (i == 0) 
					return window;
				i -= 1;
			}
			return nullptr;
		}

		int get_index(rwm::Window* win, int start_index = 0) {
			if (window == nullptr) {
				for (int j = 0; j < cells.size(); j++) {
					int ccell = cells[j]->get_index(win, start_index);
					if (ccell < 0)
						start_index -= ccell;
					else 
						return ccell;
				}
			} else {
				if (window == win) 
					return start_index;
				return -1;
			}
			return -cells.size();
		}


		cell_index find_cell_of(rwm::Window* win) {
			if (window == nullptr) {
				for (int j = 0; j < cells.size(); j++) {
					cell_index ccell = cells[j]->find_cell_of(win);
					if (ccell.c != nullptr) {
						ccell.indices.push_back(j);
						return ccell;
					}
				}
			} else {
				if (win == window) 
					return {this->parent, {}};
			}
			return {nullptr, {}};
		}

		void insert_cell(cell* c, int j) {
			if (window != nullptr) {
				cells.push_back(new cell{window, {}, false, this});
				window = nullptr;
			}
			j = std::clamp(j, 0, (int) this->cells.size());
			cells.insert(cells.begin() + j, c);
			c->parent = this;
		}

		void move_tile(rwm::ivec2 d) {
			cell_index selected_cell = find_cell_of(P_SEL_WIN);
			cell_index original_cell = selected_cell;
			
			for (int i = 0; i < selected_cell.indices.size(); i++) {
				if (selected_cell.c == nullptr)
					selected_cell.c = this;
				int new_i = selected_cell.indices[i] + ((d.y != 0) ? d.y : d.x);

				if ((selected_cell.c->vertical != (d.x != 0)) && 0 <= new_i && new_i < selected_cell.c->cells.size()) {
					alt_pressed = false;
					selected_cell.c->cells[new_i]->insert_cell(original_cell.c->cells[original_cell.indices[0]], new_i);
					original_cell.c->cells.erase(original_cell.c->cells.begin() + original_cell.indices[0]);
					if (original_cell.c->cells.size() == 1) {
						if (original_cell.c->cells[0]->window) {
							original_cell.c->window = original_cell.c->cells[0]->window;
							original_cell.c->cells.clear();
						} else {
							original_cell.c->vertical = original_cell.c->cells[0]->vertical;
							original_cell.c->cells = original_cell.c->cells[0]->cells;
							for (cell* c: original_cell.c->cells)
								c->parent = original_cell.c;
						}
					}

					return;
				}

				selected_cell.c = selected_cell.c->parent;
			}
		}

		void set_selected_cell(cell* c, int j) {
			if (c->window)
				set_selected(c->window);
			else if (c->cells.size() > 0) {
				j = std::min(j, (int) (c->cells.size() - 1));
				set_selected_cell(c->cells[j], 0);
			}
		}

		void move_window_selection(rwm::ivec2 d) {
			// Assuming d has at most one entry {-1, 1} and the other entry 0
			if (!rwm::selected_window)
				return;
			if (tiled_mode == TILED) {
				cell_index selected_cell = find_cell_of(P_SEL_WIN);
				
				for (int i = 0; i < selected_cell.indices.size(); i++) {
					if (selected_cell.c == nullptr)
						selected_cell.c = this;
					int new_i = selected_cell.indices[i] + ((d.y != 0) ? d.y : d.x);

					if ((selected_cell.c->vertical != (d.x != 0)) && 0 <= new_i && new_i < selected_cell.c->cells.size()) {
						alt_pressed = false;
						set_selected_cell(selected_cell.c->cells[new_i], selected_cell.indices[0]);
						return;
					}
			
					selected_cell.c = selected_cell.c->parent;
				}
			} else {
				int i = get_index(P_SEL_WIN);
				int new_i = i + d.x + d.y;
				if (new_i >= 0) {
					rwm::Window* new_win = get(new_i);
					set_selected(new_win);
				}
			}
		}

		void do_tiled_mode(rwm::ivec2 pos, rwm::ivec2 size) {
			if (window == nullptr) {
				for (int i = 0; i < cells.size(); i++) {
					rwm::ivec2 new_pos;
					rwm::ivec2 new_size;
					if (vertical) {
						new_size = {size.y / (int)cells.size(), size.x};
						new_pos = {pos.y + new_size.y * i, pos.x};
					} else { 
						new_size = {size.y, size.x / (int)cells.size()};
						new_pos = {pos.y, pos.x + new_size.x * i};
					}
					cells[i]->do_tiled_mode(new_pos, new_size);
				}
			} else {
				if (tiled_mode == TABBED) {
					window->status |= rwm::MAXIMIZED;
					if (window != P_SEL_WIN)
						window->status |= rwm::HIDDEN;
					else
						window->status &= ~rwm::HIDDEN;
					window->maximize();
				} else {
					window->status &= ~(rwm::MAXIMIZED | rwm::HIDDEN);
					rwm::ivec2 old_size = window->size;
					rwm::ivec2 old_pos = window->pos;
					window->resize(size);
					window->move(pos);
					window->size = old_size;
					window->pos = old_pos;
				}
			}

		}

		void apply_tiled_mode() {
			if (tiled_mode) 
				do_tiled_mode({0, 0}, {getmaxy(stdscr) - 1, getmaxx(stdscr)});
			else for (rwm::Window* win : rwm::windows) 
				win->maximize();
		}
	};

	cell root_cell = {nullptr, {}, false, nullptr};

	void new_win(rwm::Window* win) {
		cell::cell_index i;
		if (SEL_WIN > 0) 
			i = root_cell.find_cell_of(P_SEL_WIN);
		else 
			i = {nullptr, {}};
		rwm::windows.push_back(win);
		i.c = i.c ? i.c : &root_cell;
		i.c->add(P_SEL_WIN, i);
	}

	void close_window(rwm::Window* win) {
		root_cell.remove(win);
	}

	struct Widget {
		std::string draw_cmd;
		std::string win_on_click;
		rwm::ivec2 win_dim;
		int widget_size = 0;

		std::string get_str() {
			// Source: https://stackoverflow.com/questions/478898/how-do-i-execute-a-command-and-get-the-output-of-the-command-within-c-using-po
			char buffer[128];
			std::string result = "";
			FILE* pipe = popen(draw_cmd.c_str(), "r");
			if (!pipe) {
				rwm::print_debug("popen() failed!");
				return "ERROR|";
			}
			try {
				while (fgets(buffer, sizeof buffer, pipe) != NULL)
					result += buffer;
			} catch (...) {
				pclose(pipe);
				throw;
			}
			pclose(pipe);
			result.erase(std::remove(result.begin(), result.end(), '\n'), result.cend());
			widget_size = result.size();
			return result;
		}
	};
	std::vector<Widget> widgets{};

	void draw_widgets() {
		std::string w_string = "";
		for (Widget& w : widgets) 
			w_string += w.get_str();
		attron(A_REVERSE);
		mvaddstr(getmaxy(stdscr) - 1, getmaxx(stdscr) - rwm::utf8length(w_string) - 1, w_string.c_str());
		attroff(A_REVERSE);
	}

	void init_widgets() {
		std::ifstream file;
		std::string line;
		std::string out;
		file.open(rwm_config + "/widgets.cfg");
		while (getline(file, line)) {
			try {
				std::stringstream line_ss(line);
				Widget w;
				std::string col1, col2;
				getline(line_ss, w.draw_cmd, '\t');
				getline(line_ss, w.win_on_click, '\t');
				getline(line_ss, col1, '\t');
				getline(line_ss, col2, '\t');
				w.win_dim.y = std::stoi(col1);
				w.win_dim.x = std::stoi(col2);
				widgets.push_back(w);
			} catch (...) {
				widgets.push_back({"echo ERROR\\|", "", {0, 0}});
			}
		}
	}

	void draw_taskbar() {
		std::string bar = std::string(getmaxx(stdscr), ' ');
		attron(A_REVERSE);
		rwm::set_color_vga(stdscr, theme[1], theme[0]);
		mvaddstr(getmaxy(stdscr) - 1, 0, bar.c_str());
		mvaddstr(getmaxy(stdscr) - 1, 0, buttons[0].c_str());
		for (rwm::Window* pwin : root_cell) {
			rwm::Window& win = *pwin;
			std::string display_title;

			if (pwin == P_SEL_WIN && rwm::selected_window) 
				attroff(A_REVERSE);
			else
				attron(A_REVERSE);

			if (win.title.length() < tab_size)
				display_title = win.title + std::string(tab_size - win.title.length(), ' ');
			else
				display_title = win.title.substr(0, tab_size - 3) + "...";

			addstr((buttons[2][0] + display_title + buttons[2][1]).c_str());
		}

		draw_widgets();
		attroff(A_REVERSE);
	}

	void draw_background() {
		if (!background)
			return;

		background->render(false);
	}

	void draw_icons() {
		if (!should_draw_icons)
			return;

		int y = 1;
		int x = (spacing.x - 3) / 2;
		int title_lines = 3;
		DIR* dirp = opendir(desktop_path.c_str());
		if (!dirp) {
			if (errno == ENOENT) {
				mkdir(desktop_path.c_str(), 0755);
			} else {
				echo();
				if (has_colors())
					use_default_colors();
				endwin();
				std::cerr << "Could not open Desktop!\n";
				exit(EXIT_FAILURE);
			}
		}
		dirent* entry;
		desktop_contents.clear();
		while ((entry = readdir(dirp)) != NULL) {
			int is_dir = (entry->d_type & DT_DIR) != 0;
			rwm::set_color_vga(stdscr, icon_colors[is_dir][1], icon_colors[is_dir][0]);
			for (int i = 0; i < 3; i++) {
				wmove(stdscr, y + i, x);
				rwm::waddstr_enc(stdscr, icons[is_dir][i], !rwm::utf8);
			}

			rwm::set_color_vga(stdscr, -1, -1);
			std::string filename = entry->d_name;
			desktop_contents.push_back(entry->d_name);
			std::string display_name;

			if (filename.length() < title_lines * (spacing.x - 1))
				display_name = filename;
			else
				display_name = filename.substr(0, title_lines * spacing.x - 6) + "...";
			
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

			if (y + spacing.y >= getmaxy(stdscr)) {
				y = 1;
				x += spacing.x;
			}
		}
		closedir(dirp);
	}

	std::string find_in_path(std::string exe) {
		std::string path = getenv("PATH");
		std::istringstream pathss(path, std::ios::in);
		for (std::string line; std::getline(pathss, line, ':');) {
			if (!access((line + '/' + exe).c_str(), F_OK))
				return line + '/' + exe;
		}
		return "";
	}

	void parse_args(int argc, char* argv[]) {
		for (int i = 1; i < argc; i++) {
			if (argv[i][0] != '-')
				continue;
			
			for (int k = 1; argv[i][k]; k++) {
				switch(argv[i][k]) {
					case 'l':
						rwm_config = getcwd(NULL, 0) + std::string("/etc");
						rwm_bin = getcwd(NULL, 0) + std::string("/bin");
						break;
					
					case 'v':
						printf("RWM version %s\nRWM Desktop version %s\n", rwm::version.c_str(), rwm_desktop::version.c_str());
						exit(0);
						break;
					
					default:
						break;
				}
			}
		}
	}

	void init() {
		if (rwm_config.empty()) {
			rwm_config = getenv("HOME") + std::string("/.config/rwm");
			rwm_bin = "/usr/bin/rwmbin";
		} else {
			erase();
		}
		setenv("RWM_CFG", rwm_config.c_str(), true);
		setenv("RWM_BIN", rwm_bin.c_str(), true);
		rwm_settings::read_envvars(rwm_config + std::string("/env.cfg"));
		rwm_settings::read_settings(rwm_config + std::string("/settings.cfg"));
		rwm_settings::read_settings(rwm_config + std::string("/theme.cfg"));

		if (!background_program.empty()) {
			rwm::ivec2 bgsize = {};
			background = new rwm::Window(stdscr, "Background: " + background_program[0], rwm::FULLSCREEN | rwm::NO_EXIT, 0, 0);
			background->flatten_buffers();
			background->launch_program(background_program);
			draw_background();
		}

		open_fifo();
		init_widgets();
		draw_icons();
		chdir(cwd.c_str());
		if (DEBUG) {
			new_win(rwm::Window::create_debug());
			should_refresh = true;
		}
		return;
	}

	void terminate() {
		close_fifo();
	}

	void render() {
		curs_set(0);
		touchwin(stdscr);
		draw_background();
		draw_icons();
		draw_taskbar();
		root_cell.apply_tiled_mode();
		wnoutrefresh(stdscr);
		should_refresh = false;
	} 

	void key_pressed(int key) {
		switch(key) {
			case '\x03':
			rwm::terminate();
			break;

			case '\t':
			if (rwm::windows.size() > 0) {
				rwm::selected_window = true;
				should_refresh = true;
			}
			break;
		}
	}

	void move_selected_win(rwm::ivec2 d) {
		if (rwm::selected_window) { 
			if (tiled_mode == WINDOWED) {
				P_SEL_WIN->move_by(d);
				should_refresh = true;
			} else if (tiled_mode == TILED) {
				root_cell.move_tile(d);
				should_refresh = true;
			}
		}
		alt_pressed = false;
	}

	void open_program(std::string input, rwm::ivec2 win_pos, rwm::ivec2 win_size) {
		int status = 0;
		if (input.back() == '@') {
			status = rwm::NO_EXIT;
			input = input.substr(0, input.size() - 1);
		}
		new_win(new rwm::Window{{shell, "-c", input}, win_pos, win_size, status});
		P_SEL_WIN->title = input;
		rwm::selected_window = true;
		noecho();
		should_refresh = true;
	}

	void d_menu() {
		std::string input = "";
		bool in_quotes = false;
		bool escape = false;
		std::string prompt_string = " >" + std::string(getmaxx(stdscr) - 14, ' ');
		mvaddstr(getmaxy(stdscr) - 1, 5, prompt_string.c_str());
		echo();
		move(getmaxy(stdscr) - 1, 7);
		curs_set(1);
		while(true) {
			int c = getch();
			switch(c) {
				case '\n': case '\r': {
					int offset = rwm::windows.size();
					open_program(input, {10 + 5 * offset, 10 + 10 * offset}, {32, 95});
					curs_set(0);
					return;
				}

				case 27:
				draw_taskbar();
				noecho();
				curs_set(0);
				return;

				case '\b': case KEY_BACKSPACE:
				if (input.size() > 0) {
					input = input.substr(0, input.size() - 1);
					addstr(" \b");
				}
				continue;

				case 32 ... 255:
				input += (char) c;
				continue;

				default:
				continue;
			}
		}
	}

	bool key_priority(int key) {
		if (alt_pressed) {
			switch (key) {
			case 13: {
				int offset = rwm::windows.size();
				new_win(new rwm::Window{{shell}, {10 + 5 * offset, 10 + 10 * offset}, win_size, 0});
				should_refresh = true;
				rwm::selected_window = true;
				alt_pressed = false;
				return true;
			}

			case 27:
			alt_pressed = false;
			return false;

			case 'r':
			resize_mode ^= KEYBOARD;
			alt_pressed = false;
			should_refresh = true;
			return true;

			case ' ': case 'e':
			tiled_mode = (tiled_mode == TILED) ? WINDOWED : TILED;
			alt_pressed = false;
			root_cell.apply_tiled_mode();
			should_refresh = true;
			return true;
		
			case 'w':
			tiled_mode = TABBED;
			alt_pressed = false;
			root_cell.apply_tiled_mode();
			should_refresh = true;
			return true;

			case 'f':
			P_SEL_WIN->status ^= rwm::FULLSCREEN;
			P_SEL_WIN->maximize();
			should_refresh = true;
			alt_pressed = false;
			return true;

			case 'm':
			P_SEL_WIN->status ^= rwm::MAXIMIZED;
			P_SEL_WIN->maximize();
			should_refresh = true;
			alt_pressed = false;
			return true;


			// Move window
			case 'L':
			move_selected_win({-1, 0});
			return true;

			case 'K':
			move_selected_win({1, 0});
			return true;

			case 'J':
			move_selected_win({0, -1});
			return true;

			case ':':
			move_selected_win({0, 1});
			return true;

			// Move selection
			case 'l':
			root_cell.move_window_selection({-1, 0});
			alt_pressed = false;
			return true;

			case 'k':
			root_cell.move_window_selection({1, 0});
			alt_pressed = false;
			return true;

			case 'j':
			root_cell.move_window_selection({0, -1});
			alt_pressed = false;
			return true;

			case ';':
			root_cell.move_window_selection({0, 1});
			alt_pressed = false;
			return true;

			// Set mode
			case 'v':
			vertical_mode = true;
			alt_pressed = false;
			return true;

			case 'h':
			vertical_mode = false;
			alt_pressed = false;
			return true;

			case 'd':
			d_menu();
			alt_pressed = false;
			return true;

			case 'Q':
			if (SEL_WIN < 0)
				return true;
			P_SEL_WIN->status |= rwm::SHOULD_CLOSE;
			rwm::close_window(SEL_WIN);
			alt_pressed = false;
			should_refresh = true;
			return true;

			case 'E':
			rwm::terminate();
			alt_pressed = false;
			return true;

			case 'R':
			rwm_desktop::init();
			should_refresh = true;
			return true;

			case 'C':
			rwm_settings::read_settings(rwm_config + "/settings.cfg");
			rwm_settings::read_settings(rwm_config + "/theme.cfg");
			should_refresh = true;
			return true;

			default:
				break;
			}
			ungetch(key);
			ungetch(27);
			return true;
		} else {
			if (resize_mode & KEYBOARD && rwm::selected_window) {
			switch (key) {
				case KEY_UP: case 'l':
				P_SEL_WIN->resize({P_SEL_WIN->size.y - 1, P_SEL_WIN->size.x});
				should_refresh = true;
				return true;
				case KEY_DOWN: case 'k':
				P_SEL_WIN->resize({P_SEL_WIN->size.y + 1, P_SEL_WIN->size.x});
				should_refresh = true;
				return true;
				case KEY_LEFT: case 'j':
				P_SEL_WIN->resize({P_SEL_WIN->size.y, P_SEL_WIN->size.x - 1});
				should_refresh = true;
				return true;
				case KEY_RIGHT: case ';':
				P_SEL_WIN->resize({P_SEL_WIN->size.y, P_SEL_WIN->size.x + 1});
				should_refresh = true;
				return true;
				default:
				break;
				}
			}
			switch(key) {
			case 27:
			alt_pressed = true;
			return true;

			// Move window
			case 566:
			move_selected_win({-1, 0});
			return true;

			case 525:
			move_selected_win({1, 0});
			return true;

			case 545:
			move_selected_win({0, -1});
			return true;

			case 560:
			move_selected_win({0, 1});
			return true;

			// Move selection
			case KEY_SR:
			root_cell.move_window_selection({-1, 0});
			return true;

			case KEY_SF:
			root_cell.move_window_selection({1, 0});
			return true;

			case KEY_SLEFT:
			root_cell.move_window_selection({0, -1});
			return true;

			case KEY_SRIGHT:
			root_cell.move_window_selection({0, 1});
			return true;
			}
		}
		return false;
	}


	void click_taskbar(int x) {
		int pos = (x - buttons[0].length()) / (tab_size + 2);
		if (x <= buttons[0].length()) {
			// [rwm] 
			d_menu();
		} else if (pos < rwm::windows.size()) {
			rwm::Window* win = root_cell.get(pos);
			if (pos == SEL_WIN && rwm::selected_window) {
				win->status |= rwm::HIDDEN;
				rwm::selected_window = false;
				curs_set(0);
			} else {
				win->status &= ~rwm::HIDDEN;
				for (int i = 0; i < rwm::windows.size(); i++) {
					if (rwm::windows[i] == win) {
						rwm::move_to_top(i);
						break;
					}
				}

				rwm::selected_window = true;
			}

			should_refresh = true;
		} else {
			// Widget clicked
			int p = getmaxx(stdscr) - 1;

			for (int i = widgets.size() - 1; i >= 0; i--) {
				p -= widgets[i].widget_size;
				if (x >= p) {
					rwm::ivec2 win_pos = {getmaxy(stdscr) - widgets[i].win_dim.y - 1, getmaxx(stdscr) - widgets[i].win_dim.x};
					open_program(widgets[i].win_on_click, win_pos, widgets[i].win_dim);
					break;
				}
			}
		}
	}

	void mouse_pressed(MEVENT event) {
		if (event.bstate & BUTTON1_RELEASED) {
			if (event.y == getmaxy(stdscr) - 1) {
				click_taskbar(event.x);
				click = {-1, -1};
			} else {
				int x, y, y_lim, pos;
				x = event.x / spacing.x;
				y = event.y / spacing.y;
				y_lim = (getmaxy(stdscr) - 2) / spacing.y;
				pos = y + y_lim * x;
				if (x == click.x && y == click.y) {
					if (pos < desktop_contents.size()) {
						int offset = rwm::windows.size();
						std::string name = desktop_contents[pos];
						rwm::spawn({find_in_path("xdg-open"), desktop_path + name});
					}

					click = {-1, -1};
				} else {
					click = {y, x};
				}
			}
		}
	}

	bool update() {
		static bool have_updated = false;
		read_fifo();
		if (tiled_mode && SEL_WIN >= 0)
			rwm::selected_window = true;

		// Refresh every minute
		auto now = std::chrono::system_clock::now().time_since_epoch();
    	long long time = std::chrono::duration_cast<std::chrono::seconds>(now).count();
		if (background) {
			int ret = background->output();
			if (ret == 1) 
				should_refresh = true;
		} 
		if (time % 60 == 0 && !have_updated)
			should_refresh = true;
		have_updated = (time % 60) == 0;

		return should_refresh;
	}

	void do_frame(rwm::Window& win, frame_state state) {
		if (rwm::utf8) {
			cchar_t left_cc;
			cchar_t right_cc;
			cchar_t top_cc;
			cchar_t bottom_cc;
			cchar_t top_left_cc;
			cchar_t top_right_cc;
			cchar_t bottom_left_cc;
			cchar_t bottom_right_cc;
			wchar_t left = rwm::utf8_to_codepoint(frame_chars[state + LEFT]);
			wchar_t right = rwm::utf8_to_codepoint(frame_chars[state + RIGHT]);
			wchar_t top = rwm::utf8_to_codepoint(frame_chars[state + TOP]);
			wchar_t bottom = rwm::utf8_to_codepoint(frame_chars[state + BOTTOM]);
			wchar_t top_left = rwm::utf8_to_codepoint(frame_chars[state + TOP_LEFT]);
			wchar_t top_right = rwm::utf8_to_codepoint(frame_chars[state + TOP_RIGHT]);
			wchar_t bottom_left = rwm::utf8_to_codepoint(frame_chars[state + BOTTOM_LEFT]);
			wchar_t bottom_right = rwm::utf8_to_codepoint(frame_chars[state + BOTTOM_RIGHT]);
			setcchar(&left_cc, &left, 0, 0, nullptr);
			setcchar(&right_cc, &right, 0, 0, nullptr);
			setcchar(&top_cc, &top, 0, 0, nullptr);
			setcchar(&bottom_cc, &bottom, 0, 0, nullptr);
			setcchar(&top_left_cc, &top_left, 0, 0, nullptr);
			setcchar(&top_right_cc, &top_right, 0, 0, nullptr);
			setcchar(&bottom_left_cc, &bottom_left, 0, 0, nullptr);
			setcchar(&bottom_right_cc, &bottom_right, 0, 0, nullptr);
			wborder_set(win.frame, &left_cc, &right_cc, &top_cc, &bottom_cc, &top_left_cc, &top_right_cc, &bottom_left_cc, &bottom_right_cc);
		} else {
			wborder(win.frame, 
				ascii_frame_chars[state + LEFT], ascii_frame_chars[state + RIGHT],
				ascii_frame_chars[state + TOP], ascii_frame_chars[state + BOTTOM],
				ascii_frame_chars[state + TOP_LEFT], ascii_frame_chars[state + TOP_RIGHT],
				ascii_frame_chars[state + BOTTOM_LEFT], ascii_frame_chars[state + BOTTOM_RIGHT]
			);
		}
	}

	bool frame_click(int i, rwm::ivec2 pos, int bstate) {
		rwm::Window& win = *rwm::windows[i];
		int f_begx, f_begy, f_maxx, f_maxy;
		getbegyx(win.frame, f_begy, f_begx);
		getmaxyx(win.frame, f_maxy, f_maxx);
		rwm::ivec2 fpos = {pos.y - f_begy, pos.x - f_begx};
		if (bstate & BUTTON1_PRESSED)  {
			if (fpos.y == 0 && fpos.x >= f_maxx - 1 - buttons[1].length() && fpos.x < f_maxx - 1) {
				should_refresh = true;
				switch (fpos.x - f_maxx + 1) {
				case -8 ... -7:
					// Minimize
					win.status |= rwm::HIDDEN;
					if (i == SEL_WIN) {
						rwm::selected_window = false;
						curs_set(0);
					}
					break;
				case -6 ... -4: 
					// Maximize
					win.status ^= rwm::MAXIMIZED;
					win.maximize();
				break;

				case -3 ... -2:
					// Close
					P_SEL_WIN->status |= rwm::SHOULD_CLOSE;
					rwm::close_window(i);
					break;
				default:
					should_refresh = false;
					break;
				}
				return false;
			}
			drag_pos = pos;
			if (!(win.status & rwm::CANNOT_RESIZE)) {
				resize_mode &= KEYBOARD;
				resize_mode |= (fpos.x == 0 || fpos.x == win.size.x - 1) ? CHANGE_X : OFF;
				resize_mode |= ((fpos.y == 0 && (resize_mode & CHANGE_X)) || fpos.y == win.size.y - 1) ? CHANGE_Y : OFF;
				resize_mode |= (fpos.y == 0) ? DRAG_Y : OFF;
				resize_mode |= (fpos.x == 0) ? DRAG_X : OFF;
			}

			wattron(win.frame, A_REVERSE);
			rwm::set_color_vga(stdscr, frame_theme[1], frame_theme[0]);
			do_frame(win, (resize_mode & (CHANGE_X | CHANGE_Y)) ? RESIZE : SELECTED);
			wattroff(win.frame, A_REVERSE);

			wrefresh(win.frame);
			wrefresh(win.win);
			win.should_refresh = false;
			return true;
		} else if (bstate & BUTTON1_RELEASED) {
			if (resize_mode & (CHANGE_X | CHANGE_Y)) {
				rwm::ivec2 new_size = {
					(resize_mode & CHANGE_Y) ? win.size.y + pos.y - drag_pos.y : win.size.y, 
					(resize_mode & CHANGE_X) ? win.size.x + pos.x - drag_pos.x : win.size.x
				};

				win.resize(new_size);
				if (resize_mode & DRAG_X)
					win.move_by({0, pos.x - drag_pos.x});

				if (resize_mode & DRAG_Y)
					win.move_by({pos.y - drag_pos.y, 0});

			} else
				win.move_by({pos.y - drag_pos.y, pos.x - drag_pos.x});

			rwm::full_refresh();
			drag_pos = {-1, -1};
			should_refresh = false;
			return false;
		}
		click = {-1, -1};
		return false;
	}

	void frame_render(rwm::Window& win, bool is_focused) {
		rwm::set_color_vga(win.frame, frame_theme[1], frame_theme[0]);
		if ((resize_mode & KEYBOARD) && is_focused) {
			wattron(win.frame, A_REVERSE);
			do_frame(win, RESIZE);
		} else {
			if (rwm::utf8 || is_focused) 
				wattron(win.frame, A_REVERSE);

			do_frame(win, is_focused ? ACTIVE : IDLE);
		}
		mvwaddstr(win.frame, 0, 1, win.title.c_str());
		if (!tiled_mode)
			mvwaddstr(win.frame, 0, getmaxx(win.frame) - 10, buttons[1].c_str());

		wattroff(win.frame, A_REVERSE);
	}

	void close_fifo() {
		if (fifofd != -1)
			close(fifofd);
		fifofd = -1;
		remove(fifo_path);
	}

	void open_fifo() {
		mkdir("/tmp/rwm", 0700);
		close_fifo();
		int ret = mkfifo(fifo_path, 0777);

		if (ret) {
			show_info("Could not create rwm fifo!");
			return;
		}

		fifofd = open(fifo_path, O_RDONLY | O_NONBLOCK);
		if (fifofd == -1) {
			show_info("Could not open rwm fifo!");
			return;
		}
	}

	void read_fifo() {
		char fifobuf[32768];
		if (fifofd <= 0)
			return;
		int ret = read(fifofd, fifobuf, sizeof fifobuf);
		if (ret <= 0)
			return;

		fifobuf[ret - 1] = 0; // remove \n character
		std::string program = fifobuf;
		int offset = rwm::windows.size();
		open_program(program, {10 + 5 * offset, 10 + 10 * offset}, {32, 95});
		//P_SEL_WIN->title = program;
		rwm::selected_window = true;
	}

	void show_info(std::string msg) {
		int offset = rwm::windows.size();
		rwm::ivec2 dim = {7, 52};
		new_win(new rwm::Window {
			{"dialog", "--no-shadow", "--msgbox", msg, std::to_string(dim.y - 2), std::to_string(dim.x - 2)}, 
			{10 + 5 * offset, 10 + 10 * offset}, 
			dim, rwm::CANNOT_RESIZE
		});
		size_t p = msg.find('\n');
		P_SEL_WIN->title = "RWM Info: " + msg.substr(0, p);
		if (DEBUG)
			rwm::print_debug(msg);
	}
}