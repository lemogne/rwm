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
	std::vector<std::string> codepage_437 = {
		"\0", "☺",  "☻", "♥", "♦", "♣", "♠", "•", "◘", "○", "◙", "♂",  "♀", "♪", "♫", "☼",
		 "►", "◄",  "↕", "‼", "¶", "§", "▬", "↨", "↑", "↓", "→", "←",  "∟", "↔", "▲", "▼", 
		 " ", "!", "\"", "#", "$", "%", "&", "'", "(", ")", "*", "+",  ",", "-", ".", "/", 
		 "0", "1",  "2", "3", "4", "5", "6", "7", "8", "9", ":", ";",  "<", "=", ">", "?", 
		 "@", "A",  "B", "C", "D", "E", "F", "G", "H", "I", "J", "K",  "L", "M", "N", "O", 
		 "P", "Q",  "R", "S", "T", "U", "V", "W", "X", "Y", "Z", "[", "\\", "]", "^", "_", 
		 "`", "a",  "b", "c", "d", "e", "f", "g", "h", "i", "j", "k",  "l", "m", "n", "o",
		 "p", "q",  "r", "s", "t", "u", "v", "w", "x", "y", "z", "{",  "|", "}", "~", "⌂", 
		 "Ç", "ü",  "é", "â", "ä", "à", "å", "ç", "ê", "ë", "è", "ï",  "î", "ì", "Ä", "Å",
		 "É", "æ",  "Æ", "ô", "ö", "ò", "û", "ù", "ÿ", "Ö", "Ü", "¢",  "£", "¥", "₧", "ƒ",
		 "á", "í",  "ó", "ú", "ñ", "Ñ", "ª", "º", "¿", "⌐", "¬", "½",  "¼", "¡", "«", "»",
		 "░", "▒",  "▓", "│", "┤", "╡", "╢", "╖", "╕", "╣", "║", "╗",  "╝", "╜", "╛", "┐",
		 "└", "┴",  "┬", "├", "─", "┼", "╞", "╟", "╚", "╔", "╩", "╦",  "╠", "═", "╬", "╧",
		 "╨", "╤",  "╥", "╙", "╘", "╒", "╓", "╫", "╪", "┘", "┌", "█",  "▄", "▌", "▐", "▀",
		 "α", "ß",  "Γ", "π", "Σ", "σ", "µ", "τ", "Φ", "Θ", "Ω", "δ",  "∞", "φ", "ε", "∩",
		 "≡", "±",  "≥", "≤", "⌠", "⌡", "÷", "≈", "°", "∙", "·", "√",  "ⁿ", "²", "■", " "
	};

	std::unordered_map<std::string, cchar_t*> acs;
	std::unordered_map<std::string, std::string> lat_sup_a;
	std::unordered_map<std::string, std::string> utf8_conv;
	std::unordered_set<std::string> available_chars;
	std::vector<std::string> reverse_video_chars;
	bool force_convert = false;
	bool utf8 = true;
	bool no_lat_sup_a = false;
	bool is_tty = false;

	// Source: StackOverflow (https://stackoverflow.com/questions/56341221/how-to-convert-a-codepoint-to-utf-8)
	std::string codepoint_to_utf8(char32_t codepoint) {
		std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> convert;
		return convert.to_bytes(&codepoint, &codepoint + 1);
	}
	void tty_get_avail_chars() {
		if (!is_tty)
			return;

		system("setfont -ou /tmp/umap");

		std::ifstream umap("/tmp/umap", std::ios::in);
		std::string line;
		while(std::getline(umap, line)) {
			size_t pos = line.find('\t');
			const char* charcp = line.substr(pos + 3).c_str();
			wchar_t cp = std::strtoul(charcp, nullptr, 16);
			available_chars.emplace(codepoint_to_utf8(cp));
		}
	}

	void init_encoding() {
		utf8 = !std::string("UTF-8").compare(nl_langinfo(CODESET));
		no_lat_sup_a = std::string("POSIX").compare(nl_langinfo(CODESET));
		is_tty = NULL == getenv("DISPLAY");
		tty_get_avail_chars();
		//utf8 = false;

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

		lat_sup_a = {
			{"¡", "!"}, {"¢", "c"}, {"¤", "*"},  {"¥", "Y"}, {"¦", "|"},
			{"§", "$"}, {"¨","\""}, {"©","(C)"}, {"ª","^a"}, {"«", "<"},  {"¬", "~"},   {"®","(R)"}, {"¯", "-"},
			{"°", "*"}, {"±", "~"}, {"²","^2"},  {"³","^3"}, {"´","\'"},  {"µ", "u"},   {"¶", "q"},  {"·", "."},
			{"¸", ","}, {"¹","^1"}, {"º", "*"},  {"»", ">"}, {"¼","1/4"}, {"½", "1/2"}, {"¾","3/4"}, {"¿", "?"},
			{"À", "A"}, {"Á", "A"}, {"Â", "A"},  {"Ã", "A"}, {"Ä", "A"},  {"Å", "A"},   {"Æ","AE"},  {"Ç", "C"},
			{"È", "E"}, {"É", "E"}, {"Ê", "E"},  {"Ë", "E"}, {"Ì", "I"},  {"Í", "I"},   {"Î", "I"},  {"Ï", "I"},
			{"Ð", "D"}, {"Ñ", "N"}, {"Ò", "O"},  {"Ó", "O"}, {"Ô", "O"},  {"Õ", "O"},   {"Ö", "O"},  {"×", "*"},
			{"Ø", "O"}, {"Ù", "U"}, {"Ú", "U"},  {"Û", "U"}, {"Ü", "U"},  {"Ý", "Y"},   {"Þ","TH"},  {"ß","ss"},
			{"à", "a"}, {"á", "a"}, {"â", "a"},  {"ã", "a"}, {"ä", "a"},  {"å", "a"},   {"æ", "a"},  {"ç", "c"},
			{"è", "e"}, {"é", "e"}, {"ê", "e"},  {"ë", "e"}, {"ì", "i"},  {"í", "i"},   {"î", "i"},  {"ï", "i"},
			{"ð", "d"}, {"ñ", "n"}, {"ò", "o"},  {"ó", "o"}, {"ô", "o"},  {"õ", "o"},   {"ö", "o"},  {"÷", "/"},
			{"ø", "o"}, {"ù", "u"}, {"ú", "u"},  {"û", "u"}, {"ü", "u"},  {"ý", "y"},   {"þ","th"},  {"ÿ", "y"},
		};

		utf8_conv = {
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
			{"Ј", "J"}, {"Љ","Lj"}, {"Њ","Nj"}, {"Ћ","dj"}, {"Ќ", "K"}, {"Ѝ", "I"}, {"Ў", "V"}, {"Џ","Dz"},
			{"А", "A"}, {"Б", "B"}, {"В", "V"}, {"Г", "G"}, {"Д", "D"}, {"Е", "E"}, {"Ж","Zh"}, {"З", "Z"},
			{"И", "I"}, {"Й", "Y"}, {"К", "K"}, {"Л", "L"}, {"М", "M"}, {"Н", "N"}, {"О", "O"}, {"П", "P"},
			{"Р", "R"}, {"С", "S"}, {"Т", "T"}, {"У", "U"}, {"Ф", "F"}, {"Х", "H"}, {"Ц", "C"}, {"Ч","Ch"},
			{"Ш","Sh"}, {"Щ","Sch"},{"Ъ","\""}, {"Ы", "Y"}, {"Ь", "'"}, {"Э", "E"}, {"Ю","Yu"}, {"Я","Ya"},
			{"а", "a"}, {"б", "b"}, {"в", "v"}, {"г", "g"}, {"д", "d"}, {"е", "e"}, {"ж","zh"}, {"з", "z"},
			{"и", "i"}, {"й", "y"}, {"к", "k"}, {"л", "l"}, {"м", "m"}, {"н", "n"}, {"о", "o"}, {"п", "p"},
			{"р", "r"}, {"с", "s"}, {"т", "t"}, {"у", "u"}, {"ф", "f"}, {"х", "h"}, {"ц", "c"}, {"ч","ch"},
			{"ш","sh"}, {"щ","sch"},{"ъ","\""}, {"ы", "y"}, {"ь", "'"}, {"э", "e"}, {"ю","yu"}, {"я","ya"},
			{"ѐ", "e"}, {"ё","yo"}, {"ђ","dj"}, {"ѓ", "g"}, {"є", "e"}, {"ѕ","dz"}, {"і", "i"}, {"ї", "i"},
			{"ј", "j"}, {"љ","lj"}, {"њ","nj"}, {"ћ","dj"}, {"ќ", "k"}, {"ѝ", "i"}, {"ў", "v"}, {"џ","dz"},
			{"Ѡ", "W"}, {"ѡ", "w"}, {"Ѣ","Ye"}, {"ѣ","ye"}, {"Ѥ","Ye"}, {"ѥ", "ye"},{"Ѧ", "A"}, {"ѧ", "a"},
			{"Ѩ","Ia"}, {"ѩ","ia"}, {"Ѫ", "E"}, {"ѫ", "e"}, {"Ѭ","Ie"}, {"ѭ","ie"}, {"Ѯ", "X"}, {"ѯ", "x"},
			{"Ѱ","Ps"}, {"ѱ","ps"}, {"Ѳ", "F"}, {"ѳ", "f"}, {"Ѵ", "V"}, {"ѵ", "v"}, {"Ѷ", "V"}, {"ѷ", "v"},
			{"Ѹ","Oy"}, {"ѹ","oy"}, {"Ѻ", "O"}, {"ѻ", "o"}, {"Ѽ", "W"}, {"ѽ", "w"}, {"Ѿ", "W"}, {"ѿ", "w"},
			{"Ҁ", "S"}, {"ҁ", "s"}, {"҂", "/"}, {"◌҃",  ""}, {"◌҄",  ""}, {"◌҅",  ""}, {"◌҆",  ""}, {"◌҇",  ""},  
			{"◌҈",  ""}, {"◌҉",  ""}, {"Ҋ", "Y"}, {"ҋ", "y"}, {"Ҍ", "'"}, {"ҍ", "'"}, {"Ҏ", "R"}, {"ҏ", "r"}, 
			{"Ґ", "G"}, {"ґ", "g"}, {"Ғ", "G"}, {"ғ", "g"}, {"Ҕ","Dj"}, {"ҕ","dj"}, {"Җ","Zh"}, {"җ","zh"},
			{"Ҙ", "Z"}, {"ҙ", "z"}, {"Қ", "Q"}, {"қ", "q"}, {"Ҝ", "K"}, {"ҝ", "k"}, {"Ҟ", "K"}, {"ҟ", "k"},
			{"Ҡ", "K"}, {"ҡ", "k"}, {"Ң", "N"}, {"ң", "n"}, {"Ҥ", "N"}, {"ҥ", "n"}, {"Ҧ", "P"}, {"ҧ", "p"},
			{"Ҩ", "O"}, {"ҩ", "o"}, {"Ҫ", "S"}, {"ҫ", "s"}, {"Ҭ", "T"}, {"ҭ", "t"}, {"Ү", "Y"}, {"ү", "y"},
			{"Ұ", "Y"}, {"ұ", "y"}, {"Ҳ", "X"}, {"ҳ", "x"}, {"Ҵ", "C"}, {"ҵ", "c"}, {"Ҷ","Ch"}, {"ҷ","ch"},
			{"Ҹ","Ch"}, {"ҹ","ch"}, {"Һ", "h"}, {"һ", "h"}, {"Ҽ", "e"}, {"ҽ", "e"}, {"Ҿ", "e"}, {"ҿ", "e"},
			{"Ӏ", "I"}, {"Ӂ","Zh"}, {"ӂ","zh"}, {"Ӄ", "K"}, {"ӄ", "k"}, {"Ӆ", "L"}, {"ӆ", "l"}, {"Ӈ", "N"},
			{"ӈ", "n"}, {"Ӊ", "N"}, {"ӊ", "n"}, {"Ӌ","Ch"}, {"ӌ","ch"}, {"Ӎ", "M"}, {"ӎ", "m"}, {"ӏ", "l"},
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
		};
	}

	void waddstr_enc(WINDOW* win, std::string string) {
		if (utf8 && !is_tty) 
			waddstr(win, string.c_str());
		else {
			std::string out = "";
			std::string utfchar = "";
			int n_cont_bytes = 0;
			for (char c : string) {
				if(utfchar.empty()) {
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
						utfchar = "";
					}
					utfchar += c;
					auto it = acs.find(utfchar);
					auto itlsa = lat_sup_a.find(utfchar);
					auto itutf8 = utf8_conv.find(utfchar);
					auto itavail = available_chars.find(utfchar);
					if (!force_convert && itavail != available_chars.end()) {
						waddstr(win, utfchar.c_str());
						utfchar = "";
					} else if (it != acs.end()) {
						cchar_t* ch = it->second;
						wadd_wch(win, ch);
						utfchar = "";
					} else if (no_lat_sup_a && itlsa != lat_sup_a.end()) {
						std::string ch = itlsa->second;
						waddstr(win, ch.c_str());
						utfchar = "";
					} else if (itutf8 != utf8_conv.end()) {
						std::string ch = itutf8->second;
						waddstr(win, ch.c_str());
						utfchar = "";
					} else if (!utfchar.compare("█")) {
						attron(A_REVERSE);
						waddch(win, ' ');
						attroff(A_REVERSE);
						utfchar = "";
					} else if (utfchar.length() >= n_cont_bytes) {
						waddstr(win, "?");
						utfchar = "";
					}
				}
			}
			waddstr(win, utfchar.c_str());
			waddstr(win, out.c_str());
		}
	}

	size_t utf8length(std::string string) {
		//if (!utf8 || force_convert) 
		//	return string.length();
		size_t l = 0;
		for (char c : string)
 			l += (c & 0xc0) != 0x80;
		return l;
	}

	std::string utf8substr(std::string string, size_t start, size_t size) {
		//if (!utf8 || force_convert) 
		//	return string.substr(start, size);
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
