#ifndef RWM_H
#define RWM_H
#include "windows.hpp"
#define MOUSE_PRESSED (BUTTON1_PRESSED | BUTTON2_PRESSED | BUTTON3_PRESSED | BUTTON4_PRESSED | BUTTON5_PRESSED)
#define MOUSE_RELEASED (BUTTON1_RELEASED | BUTTON2_RELEASED | BUTTON3_RELEASED | BUTTON4_RELEASED | BUTTON5_RELEASED)
#define MOUSE_MASK (MOUSE_PRESSED | MOUSE_RELEASED)

namespace rwm {
	extern const std::string version;            // Version
	void terminate();                            // Terminate
	void init();                                 // Initialise WM
	void move_to_top(int i);                     // Move window i to the top
	void set_selected(int i);                    // Focus window i
	int get_top_window(ivec2 pos);               // Get top (focused) window
	bool is_on_frame(ivec2 pos);                 // Check if position `pos` lies on frame of top window
	void close_window(int i);                    // Closes window i
	void full_refresh();                         // Fully refreshes the screen
	int spawn(std::vector<std::string> args);    // Spawns process
	extern int sleep_time;                       // Time that RWM waits before next refresh
}
#endif