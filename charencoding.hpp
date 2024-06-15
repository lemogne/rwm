#include <vector>
#include <string>

namespace rwm {
	extern std::vector<std::string> codepage_437;
	extern std::unordered_map<char, chtype> vt220;
	extern bool utf8;                                          // Using UTF-8?
	void waddstr_enc(WINDOW* win, std::string string);
	void init_encoding();
}