#include <iostream>
#include <fstream>
#include <vector>
#include <unistd.h>

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

struct ivec2 {
	int y;                             // Row
	int x;                             // Column
};

void draw_image(std::string filename, ivec2 pos, ivec2 size) {
	std::ifstream file;
	std::string contents;
	std::string out;
	file.open(filename, std::ios::in | std::ios::binary);
	int y = pos.y;
	std::cout << "\033[" << pos.y + 1 << ';' << pos.x + 1 << 'H';

	while (getline(file, contents)) {
		for (unsigned char c : contents) {
			if (c == '\033')
				out += '\033';
			else if (c == 13) {
				out += "\r\n";
				std::cout << out << "\033[" << pos.x << 'C';
				y++;
				out = "";
			} else if (c == 0x1a || c == 0x04)
					goto eof;
			else
				out += codepage_437[c];
		}
	}
eof:
	std::cout << out << "\r\n";
	file.close();
}

void scroll_image(std::string filename, ivec2 pos, ivec2 size, int lines_per_page, int speed) {
	std::ifstream file;
	std::string contents;
	std::string out;
	file.open(filename, std::ios::in | std::ios::binary);
	int y = pos.y;
	bool scroll_mode = false;
	std::cout << "\033[2J\033[" << pos.y + 1 << ';' << pos.x + 1 << 'H';
	while(true) {
		while (getline(file, contents)) {
			for (unsigned char c : contents) {
				if (c == '\033')
					out += '\033';
				else if (c == 13) {
					out += "\r\n";
					std::cout << out << "\033[" << pos.x << 'C';
					y++;
					if (y > pos.y + lines_per_page)
						scroll_mode = true;
					if (scroll_mode)
						usleep(speed * 1000);
					out = "";
				} else if (c == 0x1a || c == 0x04)
					goto eof;
				else
					out += codepage_437[c];
			}
		}
	eof:
		file.clear();
		file.seekg(0);
		std::cout << out << "\r\n";
	}
	file.close();
}

int main(int argc, char* argv[]) {
	if (argc < 2) {
		std::cerr << "Usage: ascii2utf8 <file> [<y_pos> <x_pos> [lines_per_page [speed [scale]]]]";
		return 1;
	}
	int x_pos = 0,
	    y_pos = 0,
	    lines_per_page = -1,
	    scale = 1,
		speed = 500;
	if (argc >= 4) {
		x_pos = std::atoi(argv[3]);
		y_pos = std::atoi(argv[2]);
		if (argc >= 5) {
			lines_per_page = std::atoi(argv[4]);
			if (argc >= 6) {
				speed = std::atoi(argv[5]);
				if (argc >= 7) 
					scale = std::atoi(argv[6]);
			}
		}
	}
	if (lines_per_page == -1)
		draw_image(argv[1], {y_pos, x_pos}, {scale, scale});
	else for (int i = 0; ; i = (i+1) % lines_per_page) {
		scroll_image(argv[1], {y_pos, x_pos}, {scale, scale}, lines_per_page, speed);
	}

	return 0;
}