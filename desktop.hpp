#ifndef RWM_DESKTOP_H
#define RWM_DESKTOP_H
#include "windows.hpp"

namespace rwm_desktop {
	void init();                                      // Initialise desktop
	void render();                                    // Render the desktop
	void key_pressed(int key);                        // Handle keypress (while focus on desktop)
	bool key_priority(int key);                       // Handle any keypress (takes precedence over anything else; returns whether key was handled)
	void mouse_pressed(MEVENT event);                 // Handle mouseclick
	bool update();                                    // Called every frame

	// Windows
	bool frame_click(int i, rwm::ivec2 pos, int bstate);             // On Window frame click; returns whether window no. i is being moved
	void frame_render(rwm::Window& win, bool is_focused);            // On Window render
}

#endif