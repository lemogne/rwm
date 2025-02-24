#ifndef RWM_RWMDESKTOP_HPP
#define RWM_RWMDESKTOP_HPP
#include "desktop.hpp"
#include <termios.h>
#include <fcntl.h>
#include <pty.h>
#include <iomanip>
#include <chrono>
#include <sstream>
#include <fstream>
#include <dirent.h>
#include "rwm.h"
#include "charencoding.hpp"
#include <unistd.h>
#include <algorithm>
#include <sys/stat.h>
#include <iostream>

#define P_SEL_WIN (rwm::windows.back())

namespace rwm_desktop {
	extern const std::string version;

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
	enum frame_part {LEFT = 0, RIGHT = 1, TOP = 2, BOTTOM = 3, TOP_LEFT = 4, TOP_RIGHT = 5, BOTTOM_LEFT = 6, BOTTOM_RIGHT = 7};
	enum frame_state {IDLE = 0, ACTIVE = 8, SELECTED = 16, RESIZE = 24};

	extern rwm::ivec2 drag_pos;
	extern int resize_mode;
	extern bool should_refresh;
	extern bool alt_pressed;
	extern int tiled_mode;
	extern bool vertical_mode;
	extern std::string desktop_path;
	extern std::string cwd;
	extern std::vector<std::string> background_program;
	extern rwm::Window* background;
	extern bool should_draw_icons;
	extern std::vector<std::string> desktop_contents;
	extern int tab_size;
	extern rwm::ivec2 spacing;
	extern rwm::ivec2 win_size;
	extern rwm::ivec2 click;
	extern std::string shell;
	extern std::string rwm_config;

	// Theme 
	extern int theme[2];
	extern int frame_theme[2];
	extern std::string buttons[3];
	extern char icons[2][3][11];
	extern char icon_colors[2][2];
	extern std::string frame_chars[32];
	extern char ascii_frame_chars[32];

	void set_selected(rwm::Window* win);

	struct cell;
	struct Widget;

	extern cell root_cell;
	extern std::vector<Widget> widgets;

	void new_win(rwm::Window* win);
	void close_window(rwm::Window* win);
	void draw_widgets();
	void init_widgets();
	void draw_taskbar();
	void draw_background();
	void draw_icons();
	void move_selected_win(rwm::ivec2 d);
	void open_program(std::string input, rwm::ivec2 win_pos, rwm::ivec2 win_size);
	void d_menu();
	void click_taskbar(int x);
	void do_frame(rwm::Window& win, frame_state state);
	void close_fifo();
	void open_fifo();
	void read_fifo();
	void show_info(std::string msg);
	std::string find_in_path(std::string exe);
}
#endif
