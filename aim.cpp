#include <cctype>
#include <format>
#include <iostream>
#include <optional>
#include <regex>
#include <string>
#include <sys/ioctl.h>
#include <system_error>
#include <termios.h>
#include <unistd.h>

/*** defines ***/

#define CTRL_KEY(k) ((k) & 0x1f)

enum class Mode {
    Normal,
    Insert,
    Visual,
    CommandLine,
};

enum EditorKey {
    BACKSPACE = 127,
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    DEL_KEY,
    HOME_KEY,
    END_KEY,
    PAGE_UP,
    PAGE_DOWN,
};

/*** terminal ***/

struct WindowSize {
    int rows;
    int cols;
};

class Terminal {
  private:
    struct termios orig_termios;

    void die(const std::string &s) {
        // clear the screen and reposition the cursor when the program dies
        write(STDOUT_FILENO, "\x1b[2J", 4);
        write(STDOUT_FILENO, "\x1b[H", 3);

        throw std::system_error(errno, std::system_category(), s);
    }

    std::optional<WindowSize> getCursorPosition() {
        std::string response;
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
        std::string pattern = R"(\x1b\[(\d+);(\d+)R)";
        std::regex r(pattern);
        std::smatch results;

        if (std::regex_match(response, results, r)) {
            return WindowSize(std::stoi(results[1]), std::stoi(results[2]));
        } else {
            return std::nullopt;
        }
    }

    std::optional<WindowSize> getWindowSize() {
        struct winsize ws;
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
            if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) {
                return std::nullopt;
            }

            return getCursorPosition();
        }
        return WindowSize(ws.ws_row, ws.ws_col);
    }

  public:
    int rows, cols;

    Terminal() {
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

        if (auto size = getWindowSize()) {
            rows = size->rows;
            cols = size->cols;
        } else {
            die("getWindowSize");
        }
    }

    ~Terminal() {
        if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1) {
            die("tcsetattr");
        }
    }

    int readKey() {
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
            } else if (seq[0] == '0') {
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
};

class Editor {
  private:
    Terminal term;
    Mode current_mode;
    int cx, cy;
    bool should_quit;

  public:
    Editor() : current_mode(Mode::Normal), cx(0), cy(0), should_quit(false) {}

    void run() {
        while (!should_quit) {
            refreshScreenDebug();
            int key = term.readKey();
            processKeyPress(key);
        }
    }

  private:
    void refreshScreenDebug() {
        write(STDOUT_FILENO, "\x1b[2J", 4);
        write(STDOUT_FILENO, "\x1b[H", 3);

        std::string status;
        status += "\r\n";
        switch (current_mode) {
        case Mode::Normal:
            status += "Normal";
            break;
        case Mode::Insert:
            status += "Insert";
            break;
        case Mode::Visual:
            status += "Visual";
            break;
        case Mode::CommandLine:
            status += "Command";
            break;
        }
        status += std::format(" | {}:{}", cy + 1, cx + 1);

        std::cout << status << std::flush;
    }

    void processKeyPress(int key) {
        switch (current_mode) {
        case Mode::Normal:
            handleNormal(key);
            break;
        case Mode::Insert:
            handleInsert(key);
            break;
        case Mode::Visual:
            handleVisual(key);
            break;
        case Mode::CommandLine:
            handleCommandLine(key);
            break;
        }
    }

    void handleNormal(int key) {
        switch (key) {
        case 'i':
        case 'I':
        case 'a':
        case 'A':
            current_mode = Mode::Insert;
            break;

        case 'v':
        case 'V':
        case CTRL_KEY('v'):
            current_mode = Mode::Visual;
            break;

        case ':':
            current_mode = Mode::CommandLine;
            break;

        case 'h':
        case ARROW_LEFT:
            if (cx > 0) {
                --cx;
            }
            break;
        case 'j':
        case ARROW_DOWN:
            ++cy;
            break;
        case 'k':
        case ARROW_UP:
            if (cy > 0) {
                --cy;
            }
            break;
        case 'l':
        case ARROW_RIGHT:
            ++cx;
            break;

        case 'Q':
            should_quit = true;
            break;
        }
    }

    void handleInsert(int key) {
        if (key == '\x1b') {
            current_mode = Mode::Normal;
            if (cx > 0) {
                --cx;
            }
        }
    }

    void handleVisual(int key) {
        if (key == '\x1b') {
            current_mode = Mode::Normal;
        } else {
            handleNormal(key);
            current_mode = Mode::Visual;
        }
    }

    void handleCommandLine(int key) {
        if (key == '\x1b') {
            current_mode = Mode::Normal;
        } else if (key == '\r') {
            // execute command
            current_mode = Mode::Normal;
        }
        // TODO
    }
};

int main() {
    try {
        Editor e;
        e.run();
    } catch (const std::system_error &e) {
        std::cerr << "Fatal error: " << e.what() << "\r\n" << std::flush;
    }
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
}
