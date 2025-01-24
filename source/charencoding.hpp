#ifndef RWM_CHARENC_H
#define RWM_CHARENC_H
#include <vector>
#include <string>
#include <ncurses.h>

namespace rwm {
	extern std::vector<std::string> codepage_437;
	extern bool utf8;                                          // Using UTF-8?
	extern bool force_convert;                                 // Forcibly convert UTF-8 to ASCII
	extern bool is_tty;
	std::string codepoint_to_utf8(char32_t codepoint);
	char32_t utf8_to_codepoint(std::string utf8char);
	void waddstr_enc(WINDOW* win, std::string string, bool forceconv = force_convert);
	size_t utf8length(std::string string);
	std::string utf8substr(std::string string, size_t start, size_t stop);
	void init_encoding();
}
#endif