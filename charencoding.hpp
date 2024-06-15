#include <vector>
#include <string>

namespace rwm {
	extern std::vector<std::string> codepage_437;
	extern bool utf8;                                          // Using UTF-8?
	extern bool force_convert;                                 // Forcibly convert UTF-8 to ASCII
	void waddstr_enc(WINDOW* win, std::string string);
	void init_encoding();
}