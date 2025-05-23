#include <vector>
#include <string>
#include <ncurses.h>
#include <unordered_map>
#include <iconv.h>
#include <langinfo.h>
#include <unistd.h>
#include <unordered_set>
#include <poll.h>
#include <fstream>
#include <codecvt>
#include <locale>



namespace rwm {
	std::unordered_map<std::string, cchar_t*> acs;
	std::unordered_map<std::string, std::string> utf8_conv;
	std::unordered_map<std::string, std::string> accented_alt;
	std::unordered_set<std::string> available_chars;
	std::vector<std::string> reverse_video_chars;
	bool force_convert = false;
	bool do_accented_alt = true;
	bool utf8 = true;
	bool is_tty = false;

	std::vector<std::string> ASCII_names = {
		"NULL", "START HEAD", "START TEXT", "END TEXT", "END FILE", "ENQUIRY", "ACK", "BELL",
		"BACKSPACE", "TAB", "NEW LINE", "VERT TAB", "FORM FEED", "RETURN", "SHIFT OUT", "SHIFT IN",
		"DATA LINK ESC", "DEV CON 1", "DEV CON 2", "DEV CON 3", "DEV CON 4", "NAK", "SYN", "END TRANS BLOCK",
		"CANCEL", "END MEDIUM", "SUB", "ESCAPE", "FILE SEP", "GROUP SEP", "RECORD SEP", "UNIT SEP"
	};

	// Source: StackOverflow (https://stackoverflow.com/questions/56341221/how-to-convert-a-codepoint-to-utf-8)
	std::string codepoint_to_utf8(char32_t codepoint) {
		std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> convert;
		return convert.to_bytes(&codepoint, &codepoint + 1);
	}

	char32_t utf8_to_codepoint(std::string utf8char) {
		std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> convert;
		return convert.from_bytes(utf8char)[0];
	}

	void tty_get_avail_chars() {
		if (!is_tty)
			return;

		int status = system("setfont -ou /tmp/umap");
		if (status) 
			return;

		std::ifstream umap("/tmp/umap", std::ios::in);
		std::string line;
		if (utf8) {
			while (std::getline(umap, line)) {
				size_t pos = line.find('\t');
				const char* charcp = line.substr(pos + 3).c_str();
				wchar_t cp = std::strtoul(charcp, nullptr, 16);
				available_chars.emplace(codepoint_to_utf8(cp));
			}
		} else if (force_convert) {
			while (std::getline(umap, line)) {
				size_t pos = line.find('\t');
				std::string codepoint = line.substr(0, pos).c_str();
				std::string charcp = line.substr(pos + 3).c_str();
				int b = std::strtoul(codepoint.c_str(), nullptr, 16);
				wchar_t cp = std::strtoul(charcp.c_str(), nullptr, 16);
				available_chars.emplace(codepoint_to_utf8(cp));
				if (b < 256)
					utf8_conv.insert_or_assign(codepoint_to_utf8(cp), std::string(1, (char)b));
			}
		}

		if (do_accented_alt)
			for (auto& it: accented_alt) {
				auto itfirst = available_chars.find(it.first);
				auto itsecond = available_chars.find(it.second);
				if (itfirst == available_chars.end() && itsecond != available_chars.end()) {
					if (utf8)
						utf8_conv.insert_or_assign(it.first, it.second);
					else
						utf8_conv.insert_or_assign(it.first, utf8_conv[it.second]);
				}
			}
		
		if (force_convert)
			available_chars.clear();
	}

	// bool is_CJK(std::string utfchar) {
	// 	char32_t c = utf8_to_codepoint(utfchar);
	// 	return (0x2e80 <= c && c <= 0xa4cf) || (0xac00 <= c && c <= 0xd7ff);
	// }

	void init_encoding() {
		utf8 = !std::string("UTF-8").compare(nl_langinfo(CODESET));
		std::string tty_name = ttyname(0);
		is_tty = tty_name.substr(0, 8) == "/dev/tty";

		acs = {
			{"─", WACS_HLINE}, {"═", WACS_D_HLINE}, {"━", WACS_T_HLINE},
			{"│", WACS_VLINE}, {"║", WACS_D_VLINE}, {"┃", WACS_T_VLINE},

			{"┌", WACS_ULCORNER},   {"╓", WACS_D_ULCORNER}, {"╒", WACS_D_ULCORNER}, {"╔", WACS_D_ULCORNER},
			{"┍", WACS_T_ULCORNER}, {"┎", WACS_T_ULCORNER}, {"┏", WACS_T_ULCORNER},

			{"┐", WACS_URCORNER},   {"╖", WACS_D_URCORNER}, {"╕", WACS_D_URCORNER}, {"╗", WACS_D_URCORNER},
			{"┑", WACS_T_URCORNER}, {"┒", WACS_T_URCORNER}, {"┓", WACS_T_URCORNER},

			{"└", WACS_LLCORNER},   {"╙", WACS_D_LLCORNER}, {"╘", WACS_D_LLCORNER}, {"╚", WACS_D_LLCORNER},
			{"┕", WACS_T_LLCORNER}, {"┖", WACS_T_LLCORNER}, {"┗", WACS_T_LLCORNER},

			{"┘", WACS_LRCORNER},   {"╜", WACS_D_LRCORNER}, {"╛", WACS_D_LRCORNER}, {"╝", WACS_D_LRCORNER},
			{"┙", WACS_T_LRCORNER}, {"┚", WACS_T_LRCORNER}, {"┛", WACS_T_LRCORNER},

			{"┬", WACS_TTEE},   {"╤", WACS_D_TTEE}, {"╥", WACS_D_TTEE}, {"╦", WACS_D_TTEE},
			{"┭", WACS_TTEE},   {"┮", WACS_TTEE},   {"┰", WACS_TTEE},
			{"┯", WACS_T_TTEE}, {"┱", WACS_T_TTEE}, {"┲", WACS_T_TTEE}, {"┳", WACS_T_TTEE},

			{"┴", WACS_BTEE},   {"╧", WACS_D_BTEE}, {"╨", WACS_D_BTEE}, {"╩", WACS_D_BTEE},
			{"┵", WACS_BTEE},   {"┶", WACS_BTEE},   {"┸", WACS_BTEE},
			{"┷", WACS_T_BTEE}, {"┹", WACS_T_BTEE}, {"┺", WACS_T_BTEE}, {"┻", WACS_T_BTEE},
			
			{"├", WACS_LTEE},   {"╟", WACS_D_LTEE}, {"╞", WACS_D_LTEE}, {"╠", WACS_D_LTEE},
			{"┝", WACS_LTEE},   {"┞", WACS_LTEE},   {"┟", WACS_LTEE},
			{"┠", WACS_T_LTEE}, {"┡", WACS_T_LTEE}, {"┢", WACS_T_LTEE}, {"┣", WACS_T_LTEE},

			{"┤", WACS_RTEE},   {"╢", WACS_D_RTEE}, {"╡", WACS_D_RTEE}, {"╣", WACS_D_RTEE},
			{"┥", WACS_RTEE},   {"┦", WACS_RTEE},   {"┧", WACS_RTEE},
			{"┨", WACS_T_RTEE}, {"┩", WACS_T_RTEE}, {"┪", WACS_T_RTEE}, {"┫", WACS_T_RTEE},	
			
			{"┼", WACS_PLUS},   {"╪", WACS_D_PLUS}, {"╫", WACS_D_PLUS}, {"╬", WACS_D_PLUS},
			{"┽", WACS_PLUS},   {"┾", WACS_PLUS},   {"┿", WACS_PLUS},   {"╀", WACS_PLUS},   {"╁", WACS_PLUS}, 
			{"╂", WACS_PLUS},   {"╃", WACS_PLUS},   {"╄", WACS_PLUS},   {"╅", WACS_PLUS},   {"╆", WACS_PLUS},
			{"╇", WACS_T_PLUS}, {"╈", WACS_T_PLUS}, {"╉", WACS_T_PLUS}, {"╊", WACS_T_PLUS}, {"╋", WACS_T_PLUS},

			{"♦", WACS_DIAMOND}, {"◆", WACS_DIAMOND},
			{"↑", WACS_UARROW},  {"↓", WACS_DARROW}, {"←", WACS_LARROW}, {"→", WACS_RARROW},
			{"≤", WACS_LEQUAL},  {"≥", WACS_GEQUAL}, {"≠", WACS_NEQUAL},
			{"±", WACS_PLMINUS}, {"π", WACS_PI},     {"°", WACS_DEGREE},
			{"£", WACS_STERLING}, {"☼", WACS_LANTERN},

			{"▤", WACS_CKBOARD}, {"▥", WACS_CKBOARD}, {"▦", WACS_CKBOARD}, {"▧", WACS_CKBOARD}, {"▨", WACS_CKBOARD}, 
			{"▩", WACS_CKBOARD}, {"▪", WACS_BULLET},  {"▬", WACS_T_HLINE}, {"◼", WACS_BLOCK},   {"◾", WACS_BULLET},
			{"▮", WACS_BLOCK},   {"◘", WACS_BULLET},  {"□", WACS_BLOCK},   {"■", WACS_BLOCK},
			{"▒", WACS_BOARD},   {"░", WACS_CKBOARD},
		};

		reverse_video_chars = {"█", "▓", "◘", "◙", "□"};

		utf8_conv = {
			{"¡", "!"}, {"¢", "c"}, {"¤", "*"}, {"¥", "Y"}, {"¦", "|"},
			{"§", "$"}, {"¨","\""}, {"©","(C)"},{"ª","^a"}, {"«","<<"}, {"¬", "~"}, {"®","(R)"},{"¯", "-"},
			{"°", "*"}, {"±", "~"}, {"²","^2"}, {"³","^3"}, {"´","\'"}, {"µ", "u"}, {"¶", "$"}, {"·", "."},
			{"¸", ","}, {"¹","^1"}, {"º", "*"}, {"»",">>"}, {"¼","1/4"},{"½","1/2"},{"¾","3/4"},{"¿", "?"},
			{"À", "A"}, {"Á", "A"}, {"Â", "A"}, {"Ã", "A"}, {"Ä", "A"}, {"Å", "A"}, {"Æ","AE"}, {"Ç", "C"},
			{"È", "E"}, {"É", "E"}, {"Ê", "E"}, {"Ë", "E"}, {"Ì", "I"}, {"Í", "I"}, {"Î", "I"}, {"Ï", "I"},
			{"Ð", "D"}, {"Ñ", "N"}, {"Ò", "O"}, {"Ó", "O"}, {"Ô", "O"}, {"Õ", "O"}, {"Ö", "O"}, {"×", "*"},
			{"Ø", "O"}, {"Ù", "U"}, {"Ú", "U"}, {"Û", "U"}, {"Ü", "U"}, {"Ý", "Y"}, {"Þ","Th"}, {"ß","ss"},
			{"à", "a"}, {"á", "a"}, {"â", "a"}, {"ã", "a"}, {"ä", "a"}, {"å", "a"}, {"æ","ae"}, {"ç", "c"},
			{"è", "e"}, {"é", "e"}, {"ê", "e"}, {"ë", "e"}, {"ì", "i"}, {"í", "i"}, {"î", "i"}, {"ï", "i"},
			{"ð", "d"}, {"ñ", "n"}, {"ò", "o"}, {"ó", "o"}, {"ô", "o"}, {"õ", "o"}, {"ö", "o"}, {"÷", "/"},
			{"ø", "o"}, {"ù", "u"}, {"ú", "u"}, {"û", "u"}, {"ü", "u"}, {"ý", "y"}, {"þ","th"}, {"ÿ", "y"},
			{"Ā", "A"}, {"ā", "a"}, {"Ă", "A"}, {"ă", "a"}, {"Ą", "A"}, {"ą", "a"}, {"Ć", "C"}, {"ć", "c"},
			{"Ĉ", "C"}, {"ĉ", "c"}, {"Ċ", "C"}, {"ċ", "c"}, {"Č", "C"}, {"č", "c"}, {"Ď", "D"}, {"ď", "d"},
			{"Đ", "D"}, {"đ", "d"}, {"Ē", "E"}, {"ē", "e"}, {"Ĕ", "E"}, {"ĕ", "e"}, {"Ė", "E"}, {"ė", "e"},
			{"Ę", "E"}, {"ę", "e"}, {"Ě", "E"}, {"ě", "e"}, {"Ĝ", "G"}, {"ĝ", "g"}, {"Ğ", "G"}, {"ğ", "g"},
			{"Ġ", "G"}, {"ġ", "g"}, {"Ģ", "G"}, {"ģ", "g"}, {"Ĥ", "H"}, {"ĥ", "h"}, {"Ħ", "H"}, {"ħ", "h"},
			{"Ĩ", "I"}, {"ĩ", "i"}, {"Ī", "I"}, {"ī", "i"}, {"Ĭ", "I"}, {"ĭ", "i"}, {"Į", "I"}, {"į", "i"},
			{"İ", "I"}, {"ı", "i"}, {"Ĳ","IJ"}, {"ĳ","ij"}, {"Ĵ", "J"}, {"ĵ", "j"}, {"Ķ", "K"}, {"ķ", "k"},
			{"ĸ", "k"}, {"Ĺ", "L"}, {"ĺ", "l"}, {"Ļ", "L"}, {"ļ", "l"}, {"Ľ","L'"}, {"ľ","l'"}, {"Ŀ", "L"},
			{"ŀ", "l"}, {"Ł", "L"}, {"ł", "l"}, {"Ń", "N"}, {"ń", "n"}, {"Ņ", "N"}, {"ņ", "n"}, {"Ň", "N"},
			{"ň", "n"}, {"ŉ","'n"}, {"Ŋ","Ng"}, {"ŋ","ng"}, {"Ō", "O"}, {"ō", "o"}, {"Ŏ", "O"}, {"ŏ", "o"},
			{"Ő", "O"}, {"ő", "o"}, {"Œ","OE"}, {"œ","oe"}, {"Ŕ", "R"}, {"ŕ", "r"}, {"Ŗ", "R"}, {"ŗ", "r"},
			{"Ř", "R"}, {"ř", "r"}, {"Ś", "S"}, {"ś", "s"}, {"Ŝ", "S"}, {"ŝ", "s"}, {"Ş", "S"}, {"ş", "s"},
			{"Š", "S"}, {"š", "s"}, {"Ţ", "T"}, {"ţ", "t"}, {"Ť", "T"}, {"ť","t'"}, {"Ŧ", "T"}, {"ŧ", "t"},
			{"Ũ", "U"}, {"ũ", "u"}, {"Ū", "U"}, {"ū", "u"}, {"Ŭ", "U"}, {"ŭ", "u"}, {"Ů", "U"}, {"ů", "u"},
			{"Ű", "U"}, {"ű", "u"}, {"Ų", "U"}, {"ų", "u"}, {"Ŵ", "W"}, {"ŵ", "w"}, {"Ŷ", "Y"}, {"ŷ", "y"},
			{"Ÿ", "Y"}, {"Ź", "Z"}, {"ź", "z"}, {"Ż", "Z"}, {"ż", "z"}, {"Ž", "Z"}, {"ž", "z"}, {"ſ", "s"},
			{"ƀ", "b"}, {"Ɓ","'B"}, {"Ƃ", "B"}, {"ƃ", "b"}, {"Ƅ", "b"}, {"ƅ", "b"}, {"Ɔ", ")"}, {"Ƈ","C'"},
			{"ƈ","c'"}, {"Ɖ", "D"}, {"Ɗ","'D"}, {"Ƌ", "d"}, {"ƌ", "d"}, {"ƍ", "g"}, {"Ǝ", "E"}, {"Ə", "e"},
			{"Ɛ", "E"}, {"Ƒ", "F"}, {"ƒ", "f"}, {"Ɠ", "G"}, {"Ɣ", "V"}, {"ƕ", "hw"},{"Ɩ", "T"}, {"Ɨ", "X"},
			{"Ƙ", "K"}, {"ƙ", "k"}, {"ƚ", "l"}, {"ƛ", "?"}, {"Ɯ", "W"}, {"Ɲ", "N"}, {"ƞ", "n"}, {"Ɵ", "0"},
			{"Ơ","O'"}, {"ơ","o'"}, {"Ƣ", "?"}, {"ƣ", "?"}, {"Ƥ", "P"}, {"ƥ", "?"}, {"Ʀ", "R"}, {"Ƨ", "S"},
			{"ƨ", "s"}, {"Ʃ", "E"}, {"ƪ", "?"}, {"ƫ", "t"}, {"Ƭ", "T"}, {"ƭ", "t"}, {"Ʈ", "T"}, {"Ư","U'"},
			{"ư","u'"}, {"Ʊ", "U"}, {"Ʋ", "V"}, {"Ƴ", "Y"}, {"ƴ", "y"}, {"Ƶ", "Z"}, {"ƶ", "z"}, {"Ʒ", "z"},
			{"Ƹ", "?"}, {"ƹ", "?"}, {"ƺ", "z"}, {"ƻ", "z"}, {"Ƽ", "5"}, {"ƽ", "5"}, {"ƾ", "?"}, {"ƿ", "w"},
			{"ǀ", "|"}, {"ǁ","||"}, {"ǂ", "#"}, {"ǃ", "!"}, {"Ǆ","DZ"}, {"ǅ","Dz"}, {"ǆ","dz"}, {"Ǉ","LJ"},
			{"ǈ","Lj"}, {"ǉ","lj"}, {"Ǌ","NJ"}, {"ǋ","Nj"}, {"ǌ","nj"}, {"Ǎ", "A"}, {"ǎ", "a"}, {"Ǐ", "I"},
			{"ǐ", "i"}, {"Ǒ", "O"}, {"ǒ", "o"}, {"Ǔ", "U"}, {"ǔ", "u"}, {"Ǖ", "U"}, {"ǖ", "u"}, {"Ǘ", "U"},
			{"ǘ", "u"}, {"Ǚ", "U"}, {"ǚ", "u"}, {"Ǜ", "U"}, {"ǜ", "u"}, {"ǝ", "e"}, {"Ǟ", "A"}, {"ǟ", "a"},
			{"Ǡ", "A"}, {"ǡ", "a"}, {"Ǣ","AE"}, {"ǣ","ae"}, {"Ǥ", "G"}, {"ǥ", "g"}, {"Ǧ", "G"}, {"ǧ", "g"},
			{"Ǩ", "K"}, {"ǩ", "k"}, {"Ǫ", "O"}, {"ǫ", "o"}, {"Ǭ", "O"}, {"ǭ", "o"}, {"Ǯ", "Z"}, {"ǯ", "z"},
			{"ǰ", "j"}, {"Ǳ","DZ"}, {"ǲ","Dz"}, {"ǳ","dz"}, {"Ǵ", "G"}, {"ǵ", "g"}, {"Ƕ","Hw"}, {"Ƿ", "W"},
			{"Ǹ", "N"}, {"ǹ", "n"}, {"Ǻ","A^n"},{"ǻ","a^n"},{"Ǽ", "A"}, {"ǽ", "a"}, {"Ǿ", "O"}, {"ǿ", "o"},
			{"Ȁ", "A"}, {"ȁ", "a"}, {"Ȃ", "A"}, {"ȃ", "a"}, {"Ȅ", "E"}, {"ȅ", "e"}, {"Ȇ", "E"}, {"ȇ", "e"},
			{"Ȉ", "I"}, {"ȉ", "i"}, {"Ȋ", "I"}, {"ȋ", "i"}, {"Ȍ", "O"}, {"ȍ", "o"}, {"Ȏ", "O"}, {"ȏ", "o"},
			{"Ȑ", "R"}, {"ȑ", "r"}, {"Ȓ", "R"}, {"ȓ", "r"}, {"Ȕ", "U"}, {"ȕ", "u"}, {"Ȗ", "U"}, {"ȗ", "u"},
			{"Ș", "S"}, {"ș", "s"}, {"Ț", "T"}, {"ț", "t"}, {"Ȝ", "Z"}, {"ȝ", "z"}, {"Ȟ", "H"}, {"ȟ", "h"},
			{"Ƞ", "N"}, {"ȡ", "d"}, {"Ȣ", "o"}, {"ȣ", "o"}, {"Ȥ", "Z"}, {"ȥ", "z"}, {"Ȧ", "A"}, {"ȧ", "a"},
			{"Ȩ", "E"}, {"ȩ", "e"}, {"Ȫ", "O"}, {"ȫ", "o"}, {"Ȭ", "O"}, {"ȭ", "o"}, {"Ȯ", "O"}, {"ȯ", "o"},
			{"Ȱ", "O"}, {"ȱ", "o"}, {"Ȳ", "Y"}, {"ȳ", "y"}, {"ȴ", "l"}, {"ȵ", "n"}, {"ȶ", "t"}, {"ȷ", "j"},
			{"ȸ","db"}, {"ȹ","qp"}, {"Ⱥ","A/"}, {"Ȼ","C/"}, {"ȼ","c/"}, {"Ƚ","L/"}, {"Ⱦ","T/"}, {"ȿ", "s"},
			{"ɀ", "z"}, {"Ɂ", "?"}, {"ɂ", "?"}, {"Ƀ", "B"}, {"Ʉ", "U"}, {"Ʌ", "L"}, {"Ɇ","E/"}, {"ɇ","e/"},
			{"Ɉ", "J"}, {"ɉ", "j"}, {"Ɋ", "A"}, {"ɋ", "a"}, {"Ɍ", "R"}, {"ɍ", "r"}, {"Ɏ", "Y"}, {"ɏ", "y"},
			{"ɐ", "a"}, {"ɑ", "a"}, {"ɒ", "a"}, {"ɓ", "b"}, {"ɔ", "o"}, {"ɕ", "c"}, {"ɖ", "d"}, {"ɗ", "d"},
			{"ɘ", "e"}, {"ə", "e"}, {"ɚ","er"}, {"ɛ", "E"}, {"ɜ", "3"}, {"ɝ","3r"}, {"ɞ", "B"}, {"ɟ", "j"},
			{"ɠ","g'"}, {"ɡ", "g"}, {"ɢ", "G"}, {"ɣ", "?"}, {"ɤ", "?"}, {"ɥ", "u"}, {"ɦ", "h"}, {"ɧ", "h"},
			{"ɨ", "i"}, {"ɩ", "i"}, {"ɪ", "I"}, {"ɫ","l-"}, {"ɬ","l~"}, {"ɭ", "l"}, {"ɮ","lz"}, {"ɯ", "w"},
			{"ɰ", "w"}, {"ɱ","mg"}, {"ɲ", "n"}, {"ɳ", "n"}, {"ɴ", "N"}, {"ɵ", "0"}, {"ɶ","OE"}, {"ɷ", "?"},
			{"ɸ","Ph"}, {"ɹ", "r"}, {"ɺ", "r"}, {"ɻ", "r"}, {"ɼ", "r"}, {"ɽ", "r"}, {"ɾ", "r"}, {"ɿ", "r"},
			{"ʀ", "R"}, {"ʁ", "R"}, {"ʂ", "s"}, {"ʃ", "S"}, {"ʄ", "S"}, {"ʅ", "s"}, {"ʆ", "S"}, {"ʇ", "t"},
			{"ʈ", "t"}, {"ʉ", "u"}, {"ʊ", "u"}, {"ʋ", "v"}, {"ʌ", "A"}, {"ʍ", "M"}, {"ʎ","ly"}, {"ʏ", "Y"},
			{"ʐ", "z"}, {"ʑ", "z"}, {"ʒ", "z"}, {"ʓ", "z"}, {"ʔ", "?"}, {"ʕ", "?"}, {"ʖ", "?"}, {"ʗ", "C"},
			{"ʘ", "0"}, {"ʙ", "B"}, {"ʚ", "?"}, {"ʛ","G'"}, {"ʜ", "H"}, {"ʝ", "j"}, {"ʞ", "k"}, {"ʟ", "L"},
			{"ʠ","q'"}, {"ʡ", "?"}, {"ʢ", "?"}, {"ʣ","dz"}, {"ʤ","dz"}, {"ʥ","dz"}, {"ʦ","ts"}, {"ʧ","tS"},
			{"ʨ","tc"}, {"ʩ","fng"},{"ʪ","ls"}, {"ʫ","lz"}, {"ʬ","ww"}, {"ʭ","[["}, {"ʮ", "y"}, {"ʯ", "y"},

			// Cyrillic
			{"Ѐ", "E"}, {"Ё", "E"}, {"Ђ","Dj"}, {"Ѓ", "G"}, {"Є", "E"}, {"Ѕ", "Dz"},{"І", "I"}, {"Ї", "I"},
			{"Ј", "J"}, {"Љ","Lj"}, {"Њ","Nj"}, {"Ћ", "C"}, {"Ќ", "K"}, {"Ѝ", "I"}, {"Ў", "V"}, {"Џ","Dz"},
			{"А", "A"}, {"Б", "B"}, {"В", "V"}, {"Г", "G"}, {"Д", "D"}, {"Е", "E"}, {"Ж","Zh"}, {"З", "Z"},
			{"И", "I"}, {"Й", "Y"}, {"К", "K"}, {"Л", "L"}, {"М", "M"}, {"Н", "N"}, {"О", "O"}, {"П", "P"},
			{"Р", "R"}, {"С", "S"}, {"Т", "T"}, {"У", "U"}, {"Ф", "F"}, {"Х", "H"}, {"Ц", "C"}, {"Ч","Ch"},
			{"Ш","Sh"}, {"Щ","Sch"},{"Ъ","\""}, {"Ы", "Y"}, {"Ь", "'"}, {"Э", "E"}, {"Ю","Yu"}, {"Я","Ya"},
			{"а", "a"}, {"б", "b"}, {"в", "v"}, {"г", "g"}, {"д", "d"}, {"е", "e"}, {"ж","zh"}, {"з", "z"},
			{"и", "i"}, {"й", "y"}, {"к", "k"}, {"л", "l"}, {"м", "m"}, {"н", "n"}, {"о", "o"}, {"п", "p"},
			{"р", "r"}, {"с", "s"}, {"т", "t"}, {"у", "u"}, {"ф", "f"}, {"х", "h"}, {"ц", "c"}, {"ч","ch"},
			{"ш","sh"}, {"щ","sch"},{"ъ","\""}, {"ы", "y"}, {"ь", "'"}, {"э", "e"}, {"ю","yu"}, {"я","ya"},
			{"ѐ", "e"}, {"ё","yo"}, {"ђ","dj"}, {"ѓ", "g"}, {"є", "e"}, {"ѕ","dz"}, {"і", "i"}, {"ї", "i"},
			{"ј", "j"}, {"љ","lj"}, {"њ","nj"}, {"ћ", "c"}, {"ќ", "k"}, {"ѝ", "i"}, {"ў", "v"}, {"џ","dz"},
			{"Ѡ", "W"}, {"ѡ", "w"}, {"Ѣ","Ye"}, {"ѣ","ye"}, {"Ѥ","Ye"}, {"ѥ", "ye"},{"Ѧ", "A"}, {"ѧ", "a"},
			{"Ѩ","Ja"}, {"ѩ","ia"}, {"Ѫ", "E"}, {"ѫ", "e"}, {"Ѭ","Je"}, {"ѭ","ie"}, {"Ѯ", "X"}, {"ѯ", "x"},
			{"Ѱ","Ps"}, {"ѱ","ps"}, {"Ѳ", "F"}, {"ѳ", "f"}, {"Ѵ", "V"}, {"ѵ", "v"}, {"Ѷ", "V"}, {"ѷ", "v"},
			{"Ѹ","Oy"}, {"ѹ","oy"}, {"Ѻ", "O"}, {"ѻ", "o"}, {"Ѽ", "W"}, {"ѽ", "w"}, {"Ѿ", "W"}, {"ѿ", "w"},
			{"Ҁ", "S"}, {"ҁ", "s"}, {"҂", "/"}, {"◌҃",  ""}, {"◌҄",  ""}, {"◌҅",  ""}, {"◌҆",  ""}, {"◌҇",  ""},  
			{"◌҈",  ""}, {"◌҉",  ""}, {"Ҋ", "Y"}, {"ҋ", "y"}, {"Ҍ", "'"}, {"ҍ", "'"}, {"Ҏ", "R"}, {"ҏ", "r"}, 
			{"Ґ", "G"}, {"ґ", "g"}, {"Ғ", "G"}, {"ғ", "g"}, {"Ҕ","Gh"}, {"ҕ","gh"}, {"Җ","Zh"}, {"җ","zh"},
			{"Ҙ", "Z"}, {"ҙ", "z"}, {"Қ", "Q"}, {"қ", "q"}, {"Ҝ", "K"}, {"ҝ", "k"}, {"Ҟ", "K"}, {"ҟ", "k"},
			{"Ҡ", "K"}, {"ҡ", "k"}, {"Ң", "N"}, {"ң", "n"}, {"Ҥ", "N"}, {"ҥ", "n"}, {"Ҧ", "P"}, {"ҧ", "p"},
			{"Ҩ", "O"}, {"ҩ", "o"}, {"Ҫ", "S"}, {"ҫ", "s"}, {"Ҭ", "T"}, {"ҭ", "t"}, {"Ү", "Y"}, {"ү", "y"},
			{"Ұ", "Y"}, {"ұ", "y"}, {"Ҳ", "X"}, {"ҳ", "x"}, {"Ҵ", "C"}, {"ҵ", "c"}, {"Ҷ","Ch"}, {"ҷ","ch"},
			{"Ҹ","Ch"}, {"ҹ","ch"}, {"Һ", "h"}, {"һ", "h"}, {"Ҽ", "e"}, {"ҽ", "e"}, {"Ҿ", "e"}, {"ҿ", "e"},
			{"Ӏ", "I"}, {"Ӂ","Zh"}, {"ӂ","zh"}, {"Ӄ", "K"}, {"ӄ", "k"}, {"Ӆ", "L"}, {"ӆ", "l"}, {"Ӈ", "N"},
			{"ӈ", "n"}, {"Ӊ", "N"}, {"ӊ", "n"}, {"Ӌ","Ch"}, {"ӌ","ch"}, {"Ӎ", "M"}, {"ӎ", "m"}, {"ӏ", "'"},
			{"Ӑ", "A"}, {"ӑ", "a"}, {"Ӓ", "A"}, {"ӓ", "a"}, {"Ӕ","AE"}, {"ӕ","ae"}, {"Ӗ", "E"}, {"ӗ", "e"},
			{"Ә", "E"}, {"ә", "e"}, {"Ӛ", "E"}, {"ӛ", "e"}, {"Ӝ","Zh"}, {"ӝ","zh"}, {"Ӟ", "Z"}, {"ӟ", "z"},
			{"Ӡ", "Z"}, {"ӡ", "z"}, {"Ӣ", "I"}, {"ӣ", "i"}, {"Ӥ", "Z"}, {"ӥ", "i"}, {"Ӧ", "O"}, {"ӧ", "o"},
			{"Ө","Oe"}, {"ө","oe"}, {"Ӫ","Oe"}, {"ӫ","oe"}, {"Ӭ", "E"}, {"ӭ", "e"}, {"Ӯ", "Y"}, {"ӯ", "y"},
			{"Ӱ", "Y"}, {"ӱ", "y"}, {"Ӳ", "Y"}, {"ӳ", "y"}, {"Ӵ","Ch"}, {"ӵ","ch"}, {"Ӷ", "G"}, {"ӷ", "g"},
			{"Ӹ", "Y"}, {"ӹ", "y"}, {"Ӻ", "G"}, {"ӻ", "g"}, {"Ӽ", "X"}, {"ӽ", "x"}, {"Ӿ", "X"}, {"ӿ", "x"},

			// Greek
			{"Ͱ", "H"}, {"ͱ", "h"}, {"Ͳ", "T"}, {"ͳ", "t"}, {"ʹ", "'"}, {"͵", ","}, {"Ͷ", "H"}, {"ͷ", "h"},
			{"ͺ", ","}, {"ͻ", "o"}, {"ͼ", "c"}, {"ͽ", "o"}, {";", ";"}, {"Ϳ", "J"}, {"΄", "'"}, {"΅", "'"},
			{"Ά","'A"}, {"·", "."}, {"Έ","'E"}, {"Ή","'H"}, {"Ί","'I"}, {"Ό","'O"}, {"Ύ","'Y"}, {"Ώ","'W"},
			{"ΐ","'i"}, {"Α", "A"}, {"Β", "B"}, {"Γ", "G"}, {"Δ", "D"}, {"Ε", "E"}, {"Ζ", "Z"}, {"Η", "H"},
			{"Θ", "8"}, {"Ι", "I"}, {"Κ", "K"}, {"Λ", "L"}, {"Μ", "M"}, {"Ν", "N"}, {"Ξ", "3"}, {"Ο", "O"},
			{"Π", "P"},             {"Ρ", "R"}, {"Σ", "S"}, {"Τ", "T"}, {"Υ", "Y"}, {"Φ", "F"}, {"Χ", "X"},
			{"Ψ", "4"}, {"Ω", "W"}, {"Ϊ", "I"}, {"Ϋ", "Y"}, {"ά","'a"}, {"έ","'e"}, {"ή","'h"}, {"ί","'i"},
			{"ΰ","'u"}, {"α", "a"}, {"β", "b"}, {"γ", "g"}, {"δ", "d"}, {"ε", "e"}, {"ζ", "z"}, {"η", "h"},
			{"θ", "8"}, {"ι", "i"}, {"κ", "k"}, {"λ", "l"}, {"μ", "m"}, {"ν", "n"}, {"ξ", "3"}, {"ο", "o"}, 
			{"π", "p"}, {"ρ", "r"}, {"ς", "s"}, {"σ", "s"}, {"τ", "t"}, {"υ", "u"}, {"φ", "f"}, {"χ", "x"}, 
			{"ψ", "4"}, {"ω", "w"}, {"ϊ", "i"}, {"ϋ", "u"}, {"ό", "o"}, {"ύ", "u"}, {"ώ", "w"}, {"Ϗ", "K"},
			{"ϐ", "B"}, {"ϑ", "8"}, {"ϒ", "Y"}, {"ϓ", "Y"}, {"ϔ", "Y"}, {"ϕ", "f"}, {"ϖ", "w"}, {"ϗ", "x"},
			{"Ϙ", "Q"}, {"ϙ", "q"}, {"Ϛ","St"}, {"ϛ","st"}, {"Ϝ", "F"}, {"ϝ", "f"}, {"Ϟ", "S"}, {"ϟ", "s"},
			{"Ϡ", "S"}, {"ϡ", "s"}, {"Ϣ", "W"}, {"ϣ", "w"}, {"Ϥ", "q"}, {"ϥ", "q"}, {"Ϧ", "h"}, {"ϧ", "s"}, 
			{"Ϩ", "S"}, {"ϩ", "s"}, {"Ϫ", "D"}, {"ϫ", "d"}, {"Ϭ", "S"}, {"ϭ", "s"}, {"Ϯ", "T"}, {"ϯ", "t"}, 
			{"ϰ", "x"}, {"ϱ", "r"}, {"ϲ", "c"}, {"ϳ", "j"}, {"ϴ", "8"}, {"ϵ", "e"}, {"϶", "e"}, {"Ϸ","Th"},
			{"ϸ","th"}, {"Ϲ", "C"}, {"Ϻ", "M"}, {"ϻ", "m"}, {"ϼ", "r"}, {"Ͻ", "O"}, {"Ͼ", "C"}, {"Ͽ", "O"},
			
			// Arabic
			{"؀", "#"}, {"؝", ""}, {"؂", "*"}, {"؃", "s"},  {"؄", ""},  {"؅", "#"}, {"؆", "3r"}, {"؇", "4r"}, {"؈", "r"}, {"؉", "%."}, {"؊", "%.."}, {"؋", "A"}, {"،", ","}, {"؝", "/"}, {"؎", " "}, {"؝", " "},
			{"؝", "PBUH"}, {"ؑ", "PBUH"}, {"ؒ", ""}, {"ؓ", ""}, {"ؔ", ""}, {"ؕ", ";"}, {"ؖ", "'y"}, {"ؗ", "^z"}, {"ؘ", "^a"}, {"ؙ", "^u"}, {"ؚ", "^i"}, {"؛", ";"}, {"؝", "-"}, {"؞", "."}, {"؟", "?"},
			{"ؠ", "y"}, {"ء", "~"}, {"آ", "a"}, {"أ", "a"}, {"ؤ", "wa"}, {"إ", "a"}, {"ئ", "ya"}, {"ا", "a"}, {"ب", "b"}, {"ة", "a"}, {"ت", "t"}, {"ث", "th"}, {"ج", "j"}, {"ح", "h"}, {"خ", "x"}, {"د", "d"},
			{"ذ", "dh"}, {"ر", "r"}, {"ز", "z"}, {"س", "s"}, {"ش", "sh"}, {"ص", "s'"}, {"ض", "d'"}, {"ط", "t'"}, {"ظ", "z'"}, {"ع", "3"}, {"غ", "gh"}, {"ػ", "k"}, {"ؼ", "k"}, {"ؽ", "y"}, {"ؾ", "y"}, {"ؿ", "y"},
			{"ـ", "-"}, {"ٝ", "f"}, {"ق", "q"}, {"ك", "k"}, {"ل", "l"}, {"م", "m"}, {"ن", "n"}, {"ه", "h"}, {"و", "w"}, {"ى", "a"}, {"ي", "y"}, {"ً", "an"}, {"ٌ", "un"}, {"ٝ", "in"}, {"َ", "a"}, {"ٝ", "u"},
			{"ٝ", "i"}, {"ّ", "="}, {"ْ", ""}, {"ٓ", "a"}, {"ٔ", "a"}, {"ٕ", "a"}, {"ٖ", "i"}, {"ٗ", ""}, {"٘", "n"}, {"ٙ", "e"}, {"ٚ", ""}, {"ٛ", ""}, {"ٜ", ""}, {"ٝ", ""}, {"ٞ", ""}, {"ٟ", "u"},
			{"٠", "0"}, {"١", "1"}, {"٢", "2"}, {"٣", "3"}, {"٤", "4"}, {"٥", "5"}, {"٦", "6"}, {"٧", "7"}, {"٨", "8"}, {"٩", "9"}, {"٪", "%"}, {"٫", "."}, {"٬", "'"}, {"٭", "*"}, {"ٮ", "b"}, {"ٯ", "q"},
			{"ٰ", "'"}, {"ٱ", ""}, {"ٲ", "a"}, {"ٳ", "u"}, {"ٴ", "'"}, {"ٵ", "~a"}, {"ٶ", "o"}, {"ٷ", "u"}, {"ٸ", "i"}, {"ٹ", "t"}, {"ٺ", "th"}, {"ٻ", "bh"}, {"ټ", "t"}, {"ٽ", "t"}, {"پ", "p"}, {"ٿ", "th"},
			{"ڀ", "bh"}, {"ڝ", "dz"}, {"ڂ", "dz"}, {"ڃ", "ny"}, {"ڄ", "dy"}, {"څ", "c"}, {"چ", "ch"}, {"ڇ", "ch"}, {"ڈ", "dd"}, {"ډ", "d"}, {"ڊ", "d"}, {"ڋ", "d"}, {"ڌ", "dh"}, {"ڝ", "ddh"}, {"ڎ", "c"}, {"ڝ", "dd"},
			{"ڝ", "d"}, {"ڑ", "rr"}, {"ڒ", "r"}, {"ړ", "r"}, {"ڔ", "r"}, {"ڕ", "rr"}, {"ږ", "g"}, {"ڗ", "dz"}, {"ژ", "j"}, {"ڙ", "r"}, {"ښ", "x"}, {"ڛ", "sh"}, {"ڜ", "ch"}, {"ڝ", "c"}, {"ڞ", "ch"}, {"ڟ", "ts"},
			{"ڠ", "ng"}, {"ڡ", "f"}, {"ڢ", "f"}, {"ڣ", "f"}, {"ڤ", "v"}, {"ڥ", "f"}, {"ڦ", "ph"}, {"ڧ", "q"}, {"ڨ", "q"}, {"ک", "k"}, {"ڪ", "k"}, {"ګ", "k"}, {"ڬ", "g"}, {"ڭ", "ng"}, {"ڮ", "g"}, {"گ", "g"},
			{"ڰ", "g"}, {"ڱ", "ng"}, {"ڲ", "g"}, {"ڳ", "g"}, {"ڴ", "g"}, {"ڵ", "l"}, {"ڶ", "l"}, {"ڷ", "l"}, {"ڸ", "l"}, {"ڹ", "n"}, {"ں", "n"}, {"ڻ", "n"}, {"ڼ", "n"}, {"ڽ", "n"}, {"ھ", "h"}, {"ڿ", "ny"},
			{"ۀ", "e"}, {"۝", "h"}, {"ۂ", "yi"}, {"ۃ", "h"}, {"ۄ", "o"}, {"ۅ", "oe"}, {"ۆ", "oe"}, {"ۇ", "u"}, {"ۈ", "yu"}, {"ۉ", "yu"}, {"ۊ", "u"}, {"ۋ", "v"}, {"ی", "y"}, {"۝", "ei"}, {"ێ", "e"}, {"۝", "o"},
			{"۝", "e"}, {"ۑ", "y"}, {"ے", "e"}, {"ۓ", "ai"}, {"۔", "."}, {"ە", "ae"}, {"ۖ", ";"}, {"ۗ", ";"}, {"ۘ", "^m"}, {"ۙ", "^l"}, {"ۚ", "^j"}, {"ۛ", "..."}, {"ۜ", "^s"}, {"۝", "%"}, {"۞", "*"}, {"۟", "^0"},
			{"۠", "0"}, {"ۡ", ""}, {"ۢ", "^m"}, {"ۣ", "_s"}, {"ۤ", ""}, {"ۥ", "w"}, {"ۦ", "y"}, {"ۧ", "^y"}, {"ۨ", "^n"}, {"۩", "^"}, {"۪", "."}, {"۫", "."}, {"۬", "."}, {"ۭ", "_m"}, {"ۮ", "d"}, {"ۯ", "r"},
			{"۰", "0"}, {"۱", "1"}, {"۲", "2"}, {"۳", "3"}, {"۴", "4"}, {"۵", "5"}, {"۶", "6"}, {"۷", "7"}, {"۸", "8"}, {"۹", "9"}, {"ۺ", "s"}, {"ۻ", "d"}, {"ۼ", "gh"}, {"۽", "&"}, {"۾", "men"}, {"ۿ", "h"},

			// Graphical	
			{"┄", "-"}, {"┅", "="}, {"┆", "|"}, {"┇", "|"}, {"┈", "-"}, {"┉", "-"}, {"┊", "|"}, {"┋", "|"},
			{"╌", "-"}, {"╍", "="}, {"╎", "|"}, {"╏", "|"}, {"╲","\\"}, {"╱", "/"},
 			{"▭", "="}, {"▯", "?"}, {"▢", "O"}, {"▣", "?"}, {"▫", "o"},
			{"▰", "?"}, {"▱", "?"}, {"▲", "^"}, {"△", "^"}, {"▴", "^"}, {"▵", "^"}, {"▶", ">"}, {"▷", ">"}, 
			{"▸", ">"}, {"▹", ">"}, {"►", ">"}, {"▻", ">"}, {"▼", "v"}, {"▽", "v"}, {"▾", "v"}, {"▿", "v"}, 
			{"◀", "<"}, {"◁", "<"}, {"◂", "<"}, {"◃", "<"}, {"◄", "<"}, {"◅", "<"}, {"◇", "o"}, 
			{"◈", "?"}, {"◉", "o"}, {"◊", "?"}, {"○", "o"}, {"◌", "o"}, {"◍", "o"}, {"◎", "o"}, {"●", "o"}, 
			{"◐", "o"}, {"◑", "o"}, {"◒", "o"}, {"◓", "o"}, {"◔", "o"}, {"◕", "o"}, {"◖", "c"}, {"◗", "o"}, 
			{"◙", "o"}, {"◚", "?"}, {"◛", "?"}, {"◜", "?"}, {"◝", "?"}, {"◞", "?"}, {"◟", "?"}, 
			{"◠", "^"}, {"◡", "u"}, {"◢", "?"}, {"◣", "?"}, {"◤", "?"}, {"◥", "?"}, {"◦", "o"}, {"◧", "?"}, 
			{"◨", "?"}, {"◩", "?"}, {"◪", "?"}, {"◫", "?"}, {"◬", "^"}, {"◭", "^"}, {"◮", "^"}, {"◯", "o"}, 
			{"◰", "?"}, {"◱", "?"}, {"◲", "?"}, {"◳", "?"}, {"◴", "o"}, {"◵", "o"}, {"◶", "o"}, {"◷", "o"}, 
			{"◸", "?"}, {"◹", "?"}, {"◺", "?"}, {"◿", "?"},

			// Japanese
			{"あ", "'a"}, {"い", "'i"}, {"う", "'u"}, {"え", "'e"}, {"お", "'o"}, 
			{"ぁ","\"a"}, {"ぃ","\"i"}, {"ぅ","\"u"}, {"ぇ","\"e"}, {"ぉ","\"o"}, 
			{"か", "ka"}, {"き", "ki"}, {"く", "ku"}, {"け", "ke"}, {"こ", "ko"}, 
			{"が", "ga"}, {"ぎ", "gi"}, {"ぐ", "gu"}, {"げ", "ge"}, {"ご", "go"}, 
			{"さ", "sa"}, {"し", "si"}, {"す", "su"}, {"せ", "se"}, {"そ", "so"}, 
			{"ざ", "za"}, {"じ", "zi"}, {"ず", "zu"}, {"ぜ", "ze"}, {"ぞ", "zo"}, 
			{"た", "ta"}, {"ち", "ti"}, {"つ", "tu"}, {"て", "te"}, {"と", "to"}, 
			{"だ", "da"}, {"ぢ", "di"}, {"づ", "du"}, {"で", "de"}, {"ど", "do"}, 
			{"な", "na"}, {"に", "ni"}, {"ぬ", "nu"}, {"ね", "ne"}, {"の", "no"}, 
			{"は", "ha"}, {"ひ", "hi"}, {"ふ", "hu"}, {"へ", "he"}, {"ほ", "ho"},  
			{"ば", "ba"}, {"び", "bi"}, {"ぶ", "bu"}, {"べ", "be"}, {"ぼ", "bo"}, 
			{"ぱ", "pa"}, {"ぴ", "pi"}, {"ぷ", "pu"}, {"ぺ", "pe"}, {"ぽ", "po"},
			{"ま", "ma"}, {"み", "mi"}, {"む", "mu"}, {"め", "me"}, {"も", "mo"}, 
			{"や", "ya"},               {"ゆ", "yu"},              {"よ", "yo"}, 
			{"ゃ", "ya"},               {"ゅ", "yu"},              {"ょ", "yo"}, 
			{"ら", "ra"}, {"り", "ri"}, {"る", "ru"}, {"れ", "re"}, {"ろ", "ro"}, 
			{"わ", "wa"}, {"ゐ", "wi"}, {"ゔ", "vu"}, {"ゑ", "we"}, {"を", "wo"}, 
			{"ん", "n'"}, {"ゕ", "ka"}, {"ゖ", "ke"}, {"っ", "'t"}, {"ゎ", "wa"},
			{"゙", "\""}, {"゚", "°"}, {"゛", "\""}, {"゜", "°"}, {"ゝ", "=="}, {"ゞ", "=\""}, {"ゟ", "yr"}, 

			{"゠", "=="},
			{"ア", "'A"}, {"イ", "'I"}, {"ウ", "'U"}, {"エ", "'E"}, {"オ", "'O"},
			{"ァ","\"A"}, {"ィ","\"I"}, {"ゥ","\"U"}, {"ェ","\"E"}, {"ォ","\"O"},
			{"カ", "KA"}, {"キ", "KI"}, {"ク", "KU"}, {"ケ", "KE"}, {"コ", "KO"},
			{"ガ", "GA"}, {"ギ", "GI"}, {"グ", "GU"}, {"ゲ", "GE"}, {"ゴ", "GO"},
			{"サ", "SA"}, {"シ", "SI"}, {"ス", "SU"}, {"セ", "SE"}, {"ソ", "SO"},
			{"ザ", "ZA"}, {"ジ", "ZI"}, {"ズ", "ZU"}, {"ゼ", "ZE"}, {"ゾ", "ZO"},
			{"タ", "TA"}, {"チ", "TI"}, {"ツ", "TU"}, {"テ", "TE"}, {"ト", "TO"},
			{"ダ", "DA"}, {"ヂ", "DI"}, {"ヅ", "DU"}, {"デ", "DE"}, {"ド", "DO"},
			{"ナ", "NA"}, {"ニ", "NI"}, {"ヌ", "NU"}, {"ネ", "NE"}, {"ノ", "NO"},
			{"ハ", "HA"}, {"ヒ", "HI"}, {"フ", "HU"}, {"ヘ", "HE"}, {"ホ", "HO"},
			{"バ", "BA"}, {"ビ", "BI"}, {"ブ", "BU"}, {"ベ", "BE"}, {"ボ", "BO"},
			{"パ", "PA"}, {"ピ", "PI"}, {"プ", "PU"}, {"ペ", "PE"}, {"ポ", "PO"},
			{"マ", "MA"}, {"ミ", "MI"}, {"ム", "MU"}, {"メ", "ME"}, {"モ", "MO"},
			{"ヤ", "YA"},               {"ユ", "YU"},              {"ヨ", "YO"},
			{"ャ", "YA"},               {"ュ", "YU"},              {"ョ", "YO"},
			{"ラ", "RA"}, {"リ", "RI"}, {"ル", "RU"}, {"レ", "RE"}, {"ロ", "RO"},
			{"ワ", "WA"}, {"ヰ", "WI"},               {"ヱ", "WE"}, {"ヲ", "WO"},
			{"ヷ", "VA"}, {"ヸ", "VI"}, {"ヴ", "VU"}, {"ヹ", "VE"}, {"ヺ", "VO"},
			{"ン", "N'"}, {"ヵ", "KA"}, {"ヶ", "ka"}, {"ッ", "'T"}, {"ヮ", "WA"},
			{"・", "  "}, {"ー", "--"}, {"ヽ", "=="}, {"ヾ","=\""}, {"ヿ", "KT"},
		};

		accented_alt = {
			{"Đ", "Ð"}, {"Ĳ", "Ÿ"}, {"ĳ", "ÿ"}, {"Ő", "Ö"}, {"ő", "ö"}, {"Ű", "Ü"}, {"ű", "ü"}, {"Ɣ", "γ"}, 
			{"Ɨ", "Ξ"}, {"ƚ", "ł"}, {"ƛ", "λ"}, {"Ɵ", "Θ"}, {"Ơ", "Ó"}, {"ơ", "ó"}, {"ƥ", "þ"}, {"Ʃ", "Σ"}, 
			{"Ư", "Ú"}, {"ư", "ú"}, {"Ǎ", "Ă"}, {"ǎ", "ă"}, {"Ǐ", "Ĭ"}, {"ǐ", "ĭ"}, {"Ǒ", "Ŏ"}, {"ǒ", "ŏ"}, 
			{"Ǔ", "Ŭ"}, {"ǔ", "ŭ"}, {"Ǖ", "Ü"}, {"ǖ", "ü"}, {"Ǘ", "Ü"}, {"ǘ", "ü"}, {"Ǚ", "Ü"}, {"ǚ", "ü"}, 
			{"Ǜ", "Ü"}, {"ǜ", "ü"}, {"Ǟ", "Ä"}, {"ǟ", "ä"}, {"Ǡ", "Ā"}, {"ǡ", "ā"}, {"Ǣ", "Æ"}, {"ǣ", "æ"}, 
			{"Ǭ", "Ō"}, {"ǭ", "ō"}, {"Ǯ", "Ž"}, {"ǯ", "ž"}, {"Ǻ", "Å"}, {"ǻ", "å"}, {"Ǽ", "Æ"}, {"ǽ", "æ"},
			{"Ǿ", "Ø"}, {"ǿ", "ø"},
			{"Ȁ", "Ä"}, {"ȁ", "ä"}, {"Ȃ", "Â"}, {"ȃ", "â"}, {"Ȅ", "Ë"}, {"ȅ", "ë"}, {"Ȇ", "Ê"}, {"ȇ", "ê"},
			{"Ȉ", "Ï"}, {"ȉ", "ï"}, {"Ȋ", "Î"}, {"ȋ", "î"}, {"Ȍ", "Ö"}, {"ȍ", "ö"}, {"Ȏ", "Ô"}, {"ȏ", "ô"},
			{"Ȕ", "Ü"}, {"ȕ", "ü"}, {"Ȗ", "Û"}, {"ȗ", "û"}, {"Ȣ", "Ǒ"}, {"ȣ", "ǒ"}, {"Ȧ", "Á"}, {"ȧ", "á"},
			{"Ȩ", "Ę"}, {"ȩ", "ę"}, {"Ȫ", "Ö"}, {"ȫ", "ö"}, {"Ȭ", "Õ"}, {"ȭ", "õ"}, {"Ȯ", "Ó"}, {"ȯ", "ó"},
			{"Ȱ", "Ó"}, {"ȱ", "ó"}, {"ȼ", "¢"}, {"Ƚ", "Ł"}, {"ȿ", "ş"}, {"Ʌ", "Λ"}, {"ə", "ǝ"}, {"ɚ", "ǝ"}, 
			{"ɛ", "ε"}, {"ɠ", "ǵ"}, {"ɣ", "γ"}, {"ɤ", "γ"}, {"ɩ", "ı"}, {"ɪ", "I"}, {"ɫ", "ł"}, {"ɬ", "ł"},
			{"ɸ", "Φ"}, {"ʂ", "ş"}, {"ʎ", "λ"}, {"ʒ", "Ʒ"}, {"ʓ", "Ʒ"}, {"ʘ", "Θ"}, 

			// Cyrillic
			{"Ѐ", "È"}, {"Ё", "Ë"}, {"Ђ", "Đ"}, {"Ѓ", "Ǵ"}, {"Є", "E"}, {"І", "I"}, {"Ї", "Ï"},
			{"Ћ", "Ć"}, {"Ќ", "Ḱ"}, {"Ѝ", "Ì"}, {"Ў", "Ŭ"}, {"Џ","Dž"},
			{"Ч", "Č"}, {"Ш", "Š"}, {"Щ", "Ś"}, {"Ж", "Ž"},
			{"Ю", "Ǔ"}, {"Я", "Ǎ"}, {"ж", "ž"}, {"ч", "č"}, {"ш", "š"}, {"щ", "ś"}, {"ю", "ǔ"}, {"я", "ǎ"},
			{"ѐ", "è"}, {"ё", "ë"}, {"ђ", "đ"}, {"ѓ", "ǵ"}, {"ї", "ï"}, {"ћ", "ć"}, {"ќ", "ḱ"}, {"ѝ", "ì"}, 
			{"џ","dž"}, {"Ѣ", "Ě"}, {"ѣ", "ě"}, {"Ѥ", "Ě"}, {"ѥ", "ě"}, {"Ѧ", "Ą"}, {"ѧ", "ą"}, {"Ѩ","Ją"}, 
			{"ѩ","ią"}, {"Ѫ", "Ę"}, {"ѫ", "ę"}, {"Ѭ","Ję"}, {"ѭ","ię"},
			{"Ґ", "Ǵ"}, {"ґ", "ǵ"}, {"Җ","Zh"}, {"җ","zh"}, {"Ң", "Ŋ"}, {"ң", "ŋ"},
			{"Ҫ", "Ş"}, {"ҫ", "ş"}, {"Ҭ", "Ţ"}, {"ҭ", "ţ"}, {"Ұ", "Ɏ"}, {"ұ", "ɏ"}, {"Ҳ", "Ḩ"}, {"ҳ", "ḩ"}, 
			{"Ҷ", "Č"}, {"ҷ", "č"}, {"Ӂ", "Ž"}, {"ӂ", "ž"}, {"Ӄ", "Ķ"}, {"ӄ", "ķ"}, {"Ӆ", "Ļ"}, {"ӆ", "ļ"}, 
			{"Ӈ", "Ŋ"}, {"ӈ", "ŋ"}, {"Ӊ", "Ŋ"}, {"ӊ", "ŋ"}, {"Ӌ", "Č"}, {"ӌ", "č"}, 
			{"Ӑ", "Ă"}, {"ӑ", "ă"}, {"Ӓ", "Ä"}, {"ӓ", "ä"}, {"Ӕ", "Æ"}, {"ӕ", "æ"}, {"Ӗ", "Ĕ"}, {"ӗ", "ĕ"},
			{"Ә", "Ə"}, {"ә", "ə"}, {"Ӛ", "Ə"}, {"ӛ", "ə"}, {"Ӝ", "Ž"}, {"ӝ", "ž"},
			{"Ӡ", "Ʒ"}, {"ӡ", "Ʒ"}, {"Ӣ", "Ī"}, {"ӣ", "ī"}, {"Ӥ", "Ï"}, {"ӥ", "ï"}, {"Ӧ", "Ö"}, {"ӧ", "ö"},
			{"Ө", "Ɵ"}, {"ө", "ɵ"}, {"Ӫ", "Ɵ"}, {"ӫ", "ɵ"}, {"Ӭ", "Ë"}, {"ӭ", "ë"}, {"Ӯ", "Ū"}, {"ӯ", "ū"},
			{"Ӱ", "Ü"}, {"ӱ", "ü"}, {"Ӳ", "Ű"}, {"ӳ", "ű"}, {"Ӵ", "Č"}, {"ӵ", "č"}, {"Ӷ", "Ģ"}, {"ӷ", "ģ"},
			{"Ӹ", "Ÿ"}, {"ӹ", "ÿ"}, {"Ӻ", "Ǥ"}, {"ӻ", "ǥ"},

			// Greek
			{"Ά", "Á"}, {"Έ","É"}, {"Ή","Ĥ"}, {"Ί","Í"}, {"Ό","Ó"}, {"Ύ","Ý"}, {"Ώ","Ẃ"},
			{"ΐ", "í"}, {"Ϊ", "Ï"}, {"Ϋ", "Ÿ"}, {"ά","á"}, {"έ","é"}, {"ή","ĥ"}, {"ί","í"},
			{"ΰ", "ú"}, {"ϊ", "ï"}, {"ϋ", "ü"}, {"ό", "ó"}, {"ύ", "ú"}, {"ώ", "ẃ"}, {"Ϗ", "Ķ"},
			{"ϐ", "ß"}, {"ϓ", "Ý"}, {"ϔ", "Ÿ"}, {"Ϸ","Þ"},
			{"ϸ","þ"}

			// Arabic
			,{"ش", "š"}, {"ڿ", "ň"}, {"ڃ", "ň"}, {"ڇ", "č"}, {"چ", "č"}, {"ڇ", "č"}, {"ڛ", "š"}, {"ڜ", "č"}, {"ڞ", "č"}
		};

		tty_get_avail_chars();
	}

	void waddstr_enc(WINDOW* win, std::string string, bool forceconv = force_convert) {
		if ((!is_tty || !utf8) && !forceconv) 
			waddstr(win, string.c_str());
		else {
			std::string out = "";
			std::string utfchar = "";
			int n_cont_bytes = 0;
			for (char c : string) {
				if (utfchar.empty()) {
					if (c >= 0) 
						out += c;
					else {
						waddstr(win, out.c_str());
						utfchar += c;
						out = "";
						n_cont_bytes = 
							 ((c & 0xe0) ==  0xc0) ? 2 
							:((c & 0xf0) ==  0xe0) ? 3
							:((c & 0xf8) ==  0xf0) ? 4
							:((c & 0xfc) ==  0xf8) ? 5
							:((c & 0xfe) ==  0xfc) ? 6 : 7;
					}
				} else {
					if ((c & 0xc0) != 0x80) {
						// c is not a valid continuation byte
						waddstr(win, "?");
						if (c >= 0) {
							out = c;
							utfchar = "";
						}
						else
							utfchar = c;
						continue;
					}
					utfchar += c;
					auto it = acs.find(utfchar);
					auto itutf8 = utf8_conv.find(utfchar);
					auto itavail = available_chars.find(utfchar);
					if (!force_convert && itavail != available_chars.end()) {
						waddstr(win, utfchar.c_str());
						utfchar = "";
					} else if (it != acs.end()) {
						cchar_t* ch = it->second;
						wadd_wch(win, ch);
						utfchar = "";
					} else if (!utfchar.compare("█")) {
						attron(A_REVERSE);
						waddch(win, ' ');
						attroff(A_REVERSE);
						utfchar = "";
					} else if (itutf8 != utf8_conv.end()) {
						std::string ch = itutf8->second;
						waddstr(win, ch.c_str());
						utfchar = "";
					} else if (utfchar.length() >= n_cont_bytes) {
						std::string unknown(wcwidth(utf8_to_codepoint(utfchar)), '?');
						waddstr(win, unknown.c_str());
						utfchar = "";
					}
				}
			}
			waddstr(win, utfchar.c_str());
			waddstr(win, out.c_str());
		}
	}

	size_t utf8length(std::string string) {
		if (!utf8 && !force_convert) 
			return string.length();
		size_t l = 0;
		for (char c : string)
 			l += (c & 0xc0) != 0x80;
		return l;
	}

	std::string utf8substr(std::string string, size_t start, size_t size) {
		if (!utf8 && !force_convert) 
			return string.substr(start, size);
		size_t byte_start = 0, byte_size = 0, l = 0;
		for (int i = 0; i < string.length(); i++) {
			if (l <= start)
				byte_start = i;
			if (l - start < size)
				byte_size = i - start;
 			l += (string[i] & 0xc0) != 0x80;
		}
		return string.substr(byte_start, byte_size + 1);
	}

	//void put_acs_char()
}
