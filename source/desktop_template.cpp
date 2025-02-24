#include "desktop.hpp"
#include "rwm.h"

namespace rwm_desktop {
	void init() {
		// Initialisation code
	}

	void parse_args(int argc, char* argv[]) {
		// Parse argv n stuff
	}

	void terminate() {
		// On termination
		// Called by rwm::terminate(), so do not call rwm::terminate() here
	}

	void render() {
		// Enter extra rendering code here
		refresh();
	} 

	void key_pressed(int key) {
		// Input handling goes here
		if (key == '\x03')
			rwm::terminate();
	}

	bool key_priority(int key) {
		// This handler is called before key code is passed to a window
		// Returns whether key was intercepted
		// If so, key is not sent to window
		return false;
	}

	void mouse_pressed(MEVENT event) {
		
	}

	bool update() {
		// Enter program logic here
		return false;
	}

	bool frame_click(int i, rwm::ivec2 pos, int bstate) {
		// Add window frame interaction handling code here
		return false;
	}

	void frame_render(rwm::Window& win, bool is_focused) {
		// Add window frame renderer here
	}

	void close_window(rwm::Window* win) {
		
	}
}