#include <vector>
#include <string>
#include <ncurses.h>

namespace rwm {
	extern std::vector<std::string> codepage_437;
	extern bool utf8;                                          // Using UTF-8?
	extern bool force_convert;                                 // Forcibly convert UTF-8 to ASCII
	void waddstr_enc(WINDOW* win, std::string string);
	size_t utf8length(std::string string);
	std::string utf8substr(std::string string, size_t start, size_t stop);
	void init_encoding();
}