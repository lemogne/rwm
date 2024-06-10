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
#include <unistd.h>
#include <algorithm>

#define P_SEL_WIN (rwm::windows.back())

namespace rwm_desktop {
	rwm::ivec2 drag_pos = {-1, -1};
	enum resize_modes {
		OFF = 0,
		DRAG_X = 1,
		DRAG_Y = 2,
		DRAG_XY = 3,
		CHANGE_X = 4,
		CHANGE_Y = 8,
		KEYBOARD = 16
	};
	enum tiled_modes {
		WINDOWED = 0,
		TILED = 1,
		TABBED = 2,
		STACKING = 3,
	};
	int resize_mode = OFF;
	bool should_refresh = false;
	bool alt_pressed = false;
	int tiled_mode = 0;
	bool vertical_mode = false;
	std::string desktop_path = getenv("HOME") + std::string("/Desktop/");
	std::vector<std::string> desktop_contents = {};
	int tab_size = 20;
	rwm::ivec2 spacing = {6, 10};
	rwm::ivec2 click = {-1, -1};

	// Colours 
	int theme[2] = {-1, 12};

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
			//iterator operator ++(int) { iterator copy(*this); ++i_; return copy; }

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

		iterator end() {return iterator(*this, cells.size());}

		struct cell_index {
			cell* c;
			std::vector<int> indices;
		};
		void add(rwm::Window* win, cell_index& j) {
			if (window != nullptr) {
				vertical = vertical_mode;
				cells.push_back(new cell{window, {}, false, this});
				window = nullptr;
			} else if (vertical != vertical_mode) {
				j.c->cells[j.indices.back()]->add(win, j);
				return;
			}
			cells.push_back(new cell{win, {}, false, this});
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
				vertical = vertical_mode;
				cells.push_back(new cell{window, {}, false, this});
				window = nullptr;
			}
			j = std::clamp(j, 0, (int) this->cells.size());
			cells.insert(cells.begin() + j, c);
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
					selected_cell.c->cells[new_i]->insert_cell(original_cell.c->cells[original_cell.indices[0]], selected_cell.indices[0]);
					original_cell.c->cells.erase(original_cell.c->cells.begin() + original_cell.indices[0]);
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
				do_tiled_mode({0, 0}, {stdscr->_maxy, stdscr->_maxx + 1});
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
		rwm::set_color_vga(stdscr, theme[1], theme[0]);
		mvaddstr(stdscr->_maxy, 0, bar.c_str());
		mvaddstr(stdscr->_maxy, 0, "[rwm]");
		for (rwm::Window* pwin : root_cell) {
			rwm::Window& win = *pwin;
			if (pwin == P_SEL_WIN && rwm::selected_window) 
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
		wnoutrefresh(stdscr);
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
			int is_dir = (entry->d_type & DT_DIR) != 0;
			rwm::set_color_vga(stdscr, icon_colors[is_dir][1], icon_colors[is_dir][0]);
			for (int i = 0; i < 3; i++) {
				wmove(stdscr, y + i, x);
				rwm::waddstr_enc(stdscr, icons[is_dir][i]);
			}
			rwm::set_color_vga(stdscr, -1, -1);
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
		chdir(desktop_path.c_str());
		return;
	}

	void render() {
		curs_set(0);
		touchwin(stdscr);
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


	void d_menu() {
		std::string input = "";
		bool in_quotes = false;
		bool escape = false;
		std::string prompt_string = " >" + std::string(stdscr->_maxx - 13, ' ');
		mvaddstr(stdscr->_maxy, 5, prompt_string.c_str());
		echo();
		move(stdscr->_maxy, 7);
		while(true) {
			int c = getch();
			switch(c) {
				case '\n': case '\r': {
					int status = 0;
					if (input.back() == '@') {
						status = rwm::NO_EXIT;
						input = input.substr(0, input.size() - 1);
					}

					int offset = rwm::windows.size();
					new_win(new rwm::Window{{"bash", "-c", input}, {10 + 5 * offset, 10 + 10 * offset}, {32, 95}, status});
					rwm::selected_window = true;
					draw_taskbar();
					noecho();
					should_refresh = true;
					return;
				}

				case 27:
				draw_taskbar();
				noecho();
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
				new_win(new rwm::Window{{"bash"}, {10 + 5 * offset, 10 + 10 * offset}, {32, 95}, 0});
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
			rwm::close_window(SEL_WIN);
			alt_pressed = false;
			return true;

			case 'E':
			rwm::terminate();
			alt_pressed = false;
			return true;

			default:
				break;
			}
			ungetch(key);
			ungetch(27);
			return true;
		} else {
			if (resize_mode & KEYBOARD) {
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
		int pos = (x - 4) / (tab_size + 2);
		if (x <= 4) {
			// [rwm] menu
		} else if (pos < rwm::windows.size()){
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
						new_win(new rwm::Window{{"xdg-open", desktop_path + name}, {10 + 5 * offset, 10 + 10 * offset}, {32, 95}, 0});
						P_SEL_WIN->title = name;
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
		if (tiled_mode && SEL_WIN >= 0)
			rwm::selected_window = true;
		return should_refresh;
	}

	bool frame_click(int i, rwm::ivec2 pos, int bstate) {
		rwm::Window& win = *rwm::windows[i];
		rwm::ivec2 fpos = {pos.y - win.frame->_begy, pos.x - win.frame->_begx};
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
			resize_mode &= KEYBOARD;
			resize_mode |= (fpos.x == 0 || fpos.x == win.size.x - 1) ? CHANGE_X : OFF;
			resize_mode |= ((fpos.y == 0 && (resize_mode & CHANGE_X)) || fpos.y == win.size.y - 1) ? CHANGE_Y : OFF;
			resize_mode |= (fpos.y == 0) ? DRAG_Y : OFF;
			resize_mode |= (fpos.x == 0) ? DRAG_X : OFF;
			wattron(win.frame, A_REVERSE);
			rwm::set_color_vga(stdscr, theme[1], theme[0]);
			if (resize_mode & (CHANGE_X | CHANGE_Y)) 
				box(win.frame, '*', '*');
			else 
				box(win.frame, '|', '-');
			wattroff(win.frame, A_REVERSE);
			wnoutrefresh(win.frame);
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
		rwm::set_color_vga(win.frame, theme[1], theme[0]);
		if ((resize_mode & KEYBOARD) && is_focused) {
			wattron(win.frame, A_REVERSE);
			box(win.frame, '*', '*');
		} else if (rwm::utf8) {
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
		if (!tiled_mode)
			mvwaddstr(win.frame, 0, win.frame->_maxx - 9, "[ - o x ]");
		wattroff(win.frame, A_REVERSE);
		draw_taskbar();
	}
}