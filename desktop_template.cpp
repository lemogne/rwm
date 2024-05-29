#include "desktop.hpp"
#include "rwm.h"

namespace rwm_desktop {
	void init() {
		// Initialisation code
	}

	void render() {
		// Enter extra rendering code here
		refresh();
	} 

	void key_pressed(int key) {
		// Input handling comes here
		if (key == '\x03')
			rwm::terminate();
	}

	bool key_priority(int key) {
		return false;
	}

	void mouse_pressed(MEVENT event) {
		
	}

	bool update() {
		// Enter program logic here
		return false;
	}

	bool frame_click(rwm::Window& win, rwm::ivec2 pos, int bstate) {
		// Add window frame interaction handling code here
		return false;
	}

	void frame_render(rwm::Window& win, bool is_focused) {
		// Add window frame renderer here
	}
}