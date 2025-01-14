#ifndef RWM_WINDOWS_H
#define RWM_WINDOWS_H

#include <ncurses.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <fstream>
#define SEL_WIN ((int) rwm::windows.size() - 1)
#ifdef NCURSES_EXT_COLORS
#define HAS_EXT_COLOR true
#else
#define HAS_EXT_COLOR false
#endif
#define DEBUG false

namespace rwm {
	extern std::ofstream debug_log;

	enum {
		FULLSCREEN = 1,         // Run in fullscreen mode
		FROZEN = 2,             // Freeze output
		NO_EXIT = 4,            // Do not close window on exit
		HIDDEN = 8,             // Do not show
		SHOULD_CLOSE = 16,      // Window should close
		REPORT_FOCUS = 32,      // Report focus to the application
		REPORT_MOUSE = 64,      // Report mouse position on click
		APP_CURSOR = 128,       // Application cursor keys
		INSERT = 256,           // Insert mode
		MAXIMIZED = 512,        // Window is maximized
		ZOMBIE = 1024,          // Window should be deleted but has not
	};

	enum BOLD_MODE {
		NONE,                   // Terminal does not support bold characters in any way
		BOLD,                   // Characters are actually displayed as bold
		BRIGHT_FG,              // Characters are shown as brighter
		BRIGHT_BG,              // Background is shown as brighter
		DARK_FG,                // Characters are shown as darker
		DARK_BG                 // Background is shown as darker
	};

	// Color settings
	extern int base_colors;                                    // Number of colors defined at start
	extern int max_colors;                                     // Maximum number of colors
	extern int max_color_pairs;                                // Maximum number of color pairs
	extern const uint64_t DEFAULT_COLOR;                       // Default color; usually 07 (black on white)
	extern int color_pairs;                                    // Number of color pairs currently defined
	extern int colors;                                         // Number of colors currently defined
	extern int bold_mode;                                      // How are bold (^[[1m) characters displayed (see enum BOLD_MODE) [used to display more colors]
	extern std::unordered_map<int, short> color_map;           // Map [color value] -> [color index]
	extern std::unordered_map<uint64_t, chtype> pair_map;      // Map [color value pair] -> [pair index]

	struct ivec2 {
		int y;                             // Row
		int x;                             // Column
	};

	struct parser_state {
		bool is_text = true;               // Currently reading text or escape sequence?
		bool vt220 = false;                // VT220 mode?
		std::string out = "";              // Text to output; when in escape mode, additional string/char parameters
		std::vector<int> ctrl = {};        // List of numerical values passed by control sequence
		char esc_type = 0;                 // Character after ESC
		uint64_t color = DEFAULT_COLOR;    // Current color
		short color_pair = 0;              // Current color pair (used with extended colors)
		chtype attrib;                     // Current ncurses attribute state
		ivec2 saved_cursor_pos = {0, 0};   // Saved cursor position
		int cursor = 1;                    // Current cursor state
		std::string esc_seq = "";
		bool auto_nl = false;
	};

	struct Window {
	public:
		WINDOW* frame;          // Window frame
		WINDOW* win;            // Window contents
		WINDOW* alt_frame;      // Alternate buffer window frame
		WINDOW* alt_win;        // Alternate buffer window contents
		std::string title = ""; // Frame title
		ivec2 size = {0, 0};    // Window (frame) size
		ivec2 pos = {0, 0};     // Window (frame) position
		int master;             // Master TTY file handle
		int slave;              // Slave TTY file handle (unused)
		int pid;                // Process ID
		int status;             // Window status bits
		int mouse_mode = 0;     // Current mouse reporting mode; 0 = OFF; other = see https://invisible-island.net/xterm/ctlseqs/ctlseqs.html#h2-Mouse-Tracking
		bool should_refresh = true;
	private:
		int alt_win_no = 0;     // Index of alternate window buffer used
		parser_state state{};   // Saved parser state

	// API
	public:
		Window(std::vector<std::string> args, ivec2 pos, ivec2 size, int attrib);  // Creates window
		Window(WINDOW* win, std::string title, int attrib, int master, int slave); // Creates internal window
		static Window* create_debug();                                             // Creates debug window
		void launch_program(std::vector<std::string> args);                        // Launches program with args in window
		int output();                                                              // Outputs window to main buffer
		void send(std::string msg);                                                // Send control sequence to process
		void send(char c);                                                         // Send character to process
		void render(bool is_focused);                                              // Fully renders window, including frame
		void move(ivec2 pos);                                                      // Moves window to specified coordinates (absolute)
		void move_by(ivec2 d);                                                     // Moves window by specified vector (relative)
		void resize(ivec2 size);                                                   // Resizes window to new dimensions
		void maximize();                                                           // Maximise or unmaximise window based on flags
		void flush();                                                              // Flushes output in output buffer to window
		void flatten_buffers();                                                    // Flattens output buffers into one
		int destroy();                                                             // Destroys window (use before deleting!)
	private:
	// Parser methods
		void apply_color(int c, bool bg);     // Applies color c to attributes
		void apply_color_pair();              // Applies color pair in attributes to text
		void set_attrib();                    // Sets attributes based on control values
		void do_osc();                        // Handle Operating System Control sequences
		void do_dcs();                        // Handle Device Control String sequences
		void do_private_seq(char mode);       // Handle private sequences
		void move_cursor(char mode);          // Move cursor based on input char (for external API, use ncurses wmove(win, y, x))
		void erase(char mode);                // Erase part of screen based on input char
	};
	
	extern std::vector<Window*> windows;      // Currently open windows
	extern bool selected_window;              // Is a window selected (if so, it's the top window of `windows`)
	void print_debug(std::string msg);        // Print debug message `msg` to stdscr

	void set_color_rgb(WINDOW* win, char red_fg, char green_fg, char blue_fg, char red_bg, char green_bg, char blue_bg); // Set color (24 bit RGB)
	void set_color_vga(WINDOW* win, int color_fg, int color_bg);                                                       // Set color (VGA)
}

#endif
