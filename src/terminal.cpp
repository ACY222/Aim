#include "terminal.hpp"
#include "config.hpp"
#include <optional>
#include <string>
#include <sys/ioctl.h>
#include <system_error>
#include <unistd.h>

void Terminal::die(const std::string &s) {
    throw std::system_error(errno, std::system_category(), s);
}

std::optional<WindowSize> Terminal::getCursorPosition() {
    std::string response;
    response.reserve(8);
    char c;

    // get cursor position
    // the terminal will respond: `\x1b[行号;列号R`
    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) {
        return std::nullopt;
    }
    while (read(STDIN_FILENO, &c, 1) == 1) {
        response.push_back(c);
        if (c == 'R') {
            break;
        }
    }

    if (response[0] != '\x1b' or response[1] != '[') {
        return std::nullopt;
    }
    int rows, cols;
    if (sscanf(&response.c_str()[2], "%d;%d", &rows, &cols) != 2) {
        return std::nullopt;
    }

    return WindowSize(rows, cols);
}

std::optional<WindowSize> Terminal::getWindowSize() {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) {
            return std::nullopt;
        }

        return getCursorPosition();
    }
    return WindowSize(ws.ws_row, ws.ws_col);
}

Terminal::Terminal() {
    // capture the original terminal attributes into orig_termios
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) {
        die("tcgetattr");
    }

    struct termios raw = orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

    // VMIN: sets the minimum number of bytes of input needed before
    // `read()` can return
    // VTIME: sets the maximum amount of time to wait before `read()`
    // returns, which is in tenths of a second (100ms)
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        die("tcsetattr");
    }

    try {
        if (auto size = getWindowSize()) {
            rows = size->rows;
            cols = size->cols;
        } else {
            die("getWindowSize");
        }
    } catch (...) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
        throw;
    }
}

Terminal::~Terminal() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1) {
        die("tcsetattr");
    }
}

int Terminal::readKey() {
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 and errno != EAGAIN) {
            die("read");
        }
    }

    if (c == '\x1b') {
        char seq[3];
        // try reading two more characters in a very short time
        if (read(STDIN_FILENO, &seq[0], 1) != 1) {
            return '\x1b';
        }
        if (read(STDIN_FILENO, &seq[1], 1) != 1) {
            return '\x1b';
        }

        if (seq[0] == '[') {
            if (seq[1] >= '0' and seq[1] <= '9') {
                if (read(STDIN_FILENO, &seq[2], 1) != 1) {
                    return '\x1b';
                }

                if (seq[2] == '~') {
                    switch (seq[1]) {
                    case '1':
                    case '7':
                        return HOME_KEY;
                    case '4':
                    case '8':
                        return END_KEY;
                    case '3':
                        return DEL_KEY;
                    case '5':
                        return PAGE_UP;
                    case '6':
                        return PAGE_DOWN;
                    }
                }
            } else {
                switch (seq[1]) {
                case 'A':
                    return ARROW_UP;
                case 'B':
                    return ARROW_DOWN;
                case 'C':
                    return ARROW_RIGHT;
                case 'D':
                    return ARROW_LEFT;
                }
            }
        } else if (seq[0] == 'O') {
            switch (seq[1]) {
            case 'H':
                return HOME_KEY;
            case 'F':
                return END_KEY;
            }
        }
        return '\x1b';
    } else {
        return c;
    }
}
