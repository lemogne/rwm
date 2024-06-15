#include <vector>
#include <string>
#include <ncurses.h>
#include <unordered_map>

namespace rwm {
	std::vector<std::string> codepage_437 = {
		"\0", "☺", "☻", "♥", "♦", "♣", "♠", "•", "◘", "○", "◙", "♂", "♀", "♪", "♫", "☼",
		"►", "◄", "↕", "‼", "¶", "§", "▬", "↨", "↑", "↓", "→", "←", "∟", "↔", "▲", "▼", 
		" ", "!", "\"", "#", "$", "%", "&", "'", "(", ")", "*", "+", ",", "-", ".", "/", 
		"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", ":", ";", "<", "=", ">", "?", 
		"@", "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", 
		"P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z", "[", "\\", "]", "^", "_", 
		"`", "a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m", "n", "o",
		"p", "q", "r", "s", "t", "u", "v", "w", "x", "y", "z", "{", "|", "}", "~", "⌂", 
		"Ç", "ü", "é", "â", "ä", "à", "å", "ç", "ê", "ë", "è", "ï", "î", "ì", "Ä", "Å",
		"É", "æ", "Æ", "ô", "ö", "ò", "û", "ù", "ÿ", "Ö", "Ü", "¢", "£", "¥", "₧", "ƒ",
		"á", "í", "ó", "ú", "ñ", "Ñ", "ª", "º", "¿", "⌐", "¬", "½", "¼", "¡", "«", "»",
		"░", "▒", "▓", "│", "┤", "╡", "╢", "╖", "╕", "╣", "║", "╗", "╝", "╜", "╛", "┐",
		"└", "┴", "┬", "├", "─", "┼", "╞", "╟", "╚", "╔", "╩", "╦", "╠", "═", "╬", "╧",
		"╨", "╤", "╥", "╙", "╘", "╒", "╓", "╫", "╪", "┘", "┌", "█", "▄", "▌", "▐", "▀",
		"α", "ß", "Γ", "π", "Σ", "σ", "µ", "τ", "Φ", "Θ", "Ω", "δ", "∞", "φ", "ε", "∩",
		"≡", "±", "≥", "≤", "⌠", "⌡", "÷", "≈", "°", "∙", "·", "√", "ⁿ", "²", "■", " "
	};

	std::unordered_map<std::string, chtype> acs;

	void init_encoding() {
		acs = {
			{"■", ACS_BLOCK},
			{"┴", ACS_BTEE},
			{"╧", ACS_BTEE},
			{"╨", ACS_BTEE},
			{"╩", ACS_BTEE},
			{"▒", ACS_BOARD},
			{"░", ACS_CKBOARD},
			{"↓", ACS_DARROW},
			{"°", ACS_DEGREE},
			{"♦", ACS_DIAMOND},
			{"≥", ACS_GEQUAL},
			{"─", ACS_HLINE},
			{"☼", ACS_LANTERN},
			{"←", ACS_LARROW},
			{"≤", ACS_LEQUAL},
			{"└", ACS_LLCORNER},
			{"╙", ACS_LLCORNER},
			{"╘", ACS_LLCORNER},
			{"╚", ACS_LLCORNER},
			{"┘", ACS_LRCORNER},
			{"╜", ACS_LRCORNER},
			{"╛", ACS_LRCORNER},
			{"╝", ACS_LRCORNER},
			{"├", ACS_LTEE},
			{"╟", ACS_LTEE},
			{"╞", ACS_LTEE},
			{"╠", ACS_LTEE},
			{"≠", ACS_NEQUAL},
			{"π", ACS_PI},
			{"±", ACS_PLMINUS},
			{"┼", ACS_PLUS},
			{"╪", ACS_PLUS},
			{"╫", ACS_PLUS},
			{"╬", ACS_PLUS},
			{"→", ACS_RARROW},
			{"┤", ACS_RTEE},
			{"╢", ACS_RTEE},
			{"╡", ACS_RTEE},
			{"╣", ACS_RTEE},
			{"£", ACS_STERLING},
			{"┬", ACS_TTEE},
			{"╤", ACS_TTEE},
			{"╥", ACS_TTEE},
			{"╦", ACS_TTEE},
			{"↑", ACS_UARROW},
			{"┌", ACS_ULCORNER},
			{"╓", ACS_ULCORNER},
			{"╒", ACS_ULCORNER},
			{"╔", ACS_ULCORNER},
			{"┐", ACS_URCORNER},
			{"╖", ACS_URCORNER},
			{"╕", ACS_URCORNER},
			{"╗", ACS_URCORNER},
			{"│", ACS_VLINE},
			{"╗", ACS_URCORNER},
			{"╲", '\\'},
			{"╱", '/'},
		};
	}

	bool utf8 = true;

	void waddstr_enc(WINDOW* win, std::string string) {
		if (utf8) 
			waddstr(win, string.c_str());
		else {
			std::string out = "";
			std::string utfchar = "";
			for (char c : string) {
				if(utfchar.empty()) {
					if (c >= 0) 
						out += c;
					else {
						waddstr(win, out.c_str());
						utfchar += c;
						out = "";
					}
				} else {
					utfchar += c;
					auto it = acs.find(utfchar);
					if (it != acs.end()) {
						chtype ch = it->second;
						waddch(win, ch);
						utfchar = "";
					} else if (!utfchar.compare("█")) {
						attron(A_REVERSE);
						waddch(win, ' ');
						attroff(A_REVERSE);
						utfchar = "";
					} else if (utfchar.length() > 4) {
						waddstr(win, utfchar.c_str());
						utfchar = "";
					}
				}
			}
			waddstr(win, utfchar.c_str());
			waddstr(win, out.c_str());
		}
	}

	//void put_acs_char()
}