#include <algorithm>
#include <cctype>
#include <cstdio>
#include <format>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <sys/ioctl.h>
#include <system_error>
#include <termios.h>
#include <unistd.h>
#include <vector>

/*** defines ***/

#define CTRL_KEY(k) ((k) & 0x1f)

// the width of line numbers
#define LINE_NUMBER_LEN 4
// the width opf Mode and filename in status bar
#define MODE_LEN 8
#define FILENAME_LEN 10

// how many spaces a Tab keystroke counts for
#define SOFTTABSTOP 4

enum class Mode {
    Normal,
    Insert,
    Visual,
    VisualLine,
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
        throw std::system_error(errno, std::system_category(), s);
    }

    std::optional<WindowSize> getCursorPosition() {
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
};

class Editor {
  private:
    Terminal term;
    Mode current_mode;
    int cx, cy;
    int row_off, col_off;

    std::string filename;
    std::vector<std::string> rows;
    int num_rows;
    bool should_quit;

  public:
    Editor()
        : current_mode(Mode::Normal), cx(0), cy(0), row_off(0), col_off(0),
          num_rows(0), should_quit(false) {}

    void openFile(std::string_view filepath) {
        filename = filepath;
        std::ifstream file(filename);

        if (file.is_open()) {
            std::string line;
            while (std::getline(file, line)) {
                if (!line.empty() and line.back() == '\r') {
                    line.pop_back();
                }

                rows.push_back(line);
                ++num_rows;
            }
        }
    }

    void run() {
        while (!should_quit) {
            refreshScreen();
            int key = term.readKey();
            processKeyPress(key);
        }
    }

  private:
    void scroll() {
        if (cy < row_off) { // scroll up
            row_off = cy;
        } else if (cy >= row_off + term.rows - 1) { // scroll down
            row_off = cy - term.rows + 2;
        }

        if (cx < col_off) {
            col_off = cx;
        } else if (cx >= col_off + term.cols - (LINE_NUMBER_LEN + 1)) {
            col_off = cx - term.cols + 1 + (LINE_NUMBER_LEN + 1);
        }
    }

    void setCursorStyle(std::string &ab) {
        switch (current_mode) {
        case Mode::Normal:
        case Mode::Visual:
        case Mode::VisualLine:
            ab += "\x1b[2 q";
            break;
        case Mode::Insert:
        case Mode::CommandLine:
            ab += "\x1b[6 q";
            break;
        }
    }

    void refreshScreen() {
        scroll();
        std::string ab; // append buffer

        // hide the cursor
        ab += "\x1b[?25l";
        // reposition the cursor to top-left
        ab += "\x1b[H";
        setCursorStyle(ab);

        drawRows(ab);
        drawStatusBar(ab);

        // move the cursor to (row, col)
        ab += std::format("\x1b[{};{}H", cy - row_off + 1,
                          cx - col_off + 1 + (LINE_NUMBER_LEN + 1));
        // make the cursor visible
        ab += "\x1b[?25h";
        // write to STDOUT_FILENO
        write(STDOUT_FILENO, ab.c_str(), ab.size());
    }

    void drawRows(std::string &ab) {
        for (int y = 0; y < term.rows - 1; ++y) {
            int file_row = y + row_off;
            if (file_row < num_rows) {
                ab += std::format("{:>{}} ", file_row + 1, LINE_NUMBER_LEN);
                if (col_off < static_cast<int>(rows[file_row].size())) {
                    ab.append(rows[file_row], col_off,
                              term.cols - LINE_NUMBER_LEN - 1);
                }
            } else {
                if (num_rows == 0 and y == term.rows / 3) {
                    std::string welcome = "Welcome to AIM editor!";
                    int padding_len = (term.cols - welcome.size()) / 2;
                    if (padding_len) {
                        ab.append(1, '~');
                        --padding_len;
                    }
                    ab.append(padding_len, ' ');
                    ab += welcome;
                    ab.append(padding_len, ' ');
                } else {
                    ab += "~";
                }
            }
            ab += "\x1b[K"; // clear the rest of the line
            ab += "\r\n";
        }
    }

    void drawStatusBar(std::string &ab) {
        ab += "\x1b[7m"; // reverse color

        std::string mode;
        switch (current_mode) {
        case Mode::Normal:
            mode = "Normal";
            break;
        case Mode::Insert:
            mode = "Insert";
            break;
        case Mode::Visual:
            mode = "Visual";
            break;
        case Mode::VisualLine:
            mode = "VisualLine";
            break;
        case Mode::CommandLine:
            mode = "Command";
            break;
        }

        std::string cursor_position =
            std::format("{}:{}  {:>5}", cy + 1, cx + 1, num_rows);

        int padding_len =
            term.cols - MODE_LEN - cursor_position.size() - FILENAME_LEN;
        std::string padding =
            padding_len > 0 ? std::string(padding_len, ' ') : "";

        ab += std::format("{:>{}}{:>{}}{}{}", mode, MODE_LEN, filename,
                          FILENAME_LEN, padding, cursor_position);
        ab += "\x1b[K";
        ab += "\x1b[m"; // reset color
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
        case Mode::VisualLine:
            handleVisualLine(key);
            break;
        case Mode::CommandLine:
            handleCommandLine(key);
            break;
        }
    }

    void clampCursor() {
        int high = static_cast<int>(rows[cy].size());
        if (current_mode != Mode::Insert) {
            --high;
        }
        cx = std::clamp(cx, 0, high);
        cy = std::clamp(cy, 0, num_rows - 1);
    }

    void moveCursor(int key) {
        switch (key) {
        case 'h':
        case ARROW_LEFT:
            if (cx > 0) {
                --cx;
            } else if (cy > 0) {
                --cy;
                cx = rows[cy].size();
                if (current_mode == Mode::Normal) {
                    --cx;
                }
            }
            break;
        case 'j':
        case ARROW_DOWN:
            ++cy;
            break;
        case 'k':
        case ARROW_UP:
            --cy;
            break;
        case 'l':
        case ARROW_RIGHT:
            if (cx < static_cast<int>(rows[cy].size()) - 1) {
                ++cx;
            } else if (cy < num_rows - 1) {
                ++cy;
                cx = 0;
            }
            break;
        case 'G':
            cy = num_rows - 1;
        }
        clampCursor(); // make sure that cx, cy won't be out of range
    }

    void handleNormal(int key) {
        switch (key) {
        case 'Q':
            should_quit = true;
            break;

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
        case 'j':
        case 'k':
        case 'l':
        case ARROW_LEFT:
        case ARROW_DOWN:
        case ARROW_UP:
        case ARROW_RIGHT:
        case 'G':
            moveCursor(key);
            break;

        case PAGE_UP:
            cy -= term.rows;
            break;
        case PAGE_DOWN:
            cy += term.rows;
            break;

        case HOME_KEY:
            cx = 0;
            break;
        case END_KEY:
            cx = rows[cy].size() - 1;
            break;
        }
    }

    void handleInsert(int key) {
        if (key == '\x1b') {
            current_mode = Mode::Normal;
            if (cx > 0) {
                --cx;
            }
        } else {
            // expand tab into spaces
            switch (key) {
            case '\t': {
                int num_spaces = SOFTTABSTOP - cx % SOFTTABSTOP;
                rows[cy].insert(rows[cy].begin() + cx, num_spaces, ' ');
                cx += num_spaces;
                break;
            }
            case '\r': {
                std::string last = rows[cy].substr(cx);
                rows[cy].erase(cx);
                ++cy;
                rows.insert(rows.begin() + cy, last);
                cx = 0;
                break;
            }

            case DEL_KEY:
                moveCursor(ARROW_RIGHT);
            case BACKSPACE:
            case CTRL_KEY('h'):
                if (cx == 0 and cy == 0) {
                    break;
                }

                if (cx > 0) {
                    rows[cy].erase(rows[cy].begin() + cx - 1);
                    --cx;
                } else {
                    moveCursor(ARROW_LEFT);
                    rows[cy].append(rows[cy + 1]);
                    rows.erase(rows.begin() + cy + 1);
                }
                break;

            case ARROW_LEFT:
            case ARROW_DOWN:
            case ARROW_UP:
            case ARROW_RIGHT:
                moveCursor(key);
                break;

            default:
                rows[cy].insert(rows[cy].begin() + cx, key);
                ++cx;
            }
        }
    }

    void handleVisual(int key) {
        if (key == '\x1b') {
            current_mode = Mode::Normal;
        } else {
            switch (key) {
            case 'h':
            case 'j':
            case 'k':
            case 'l':
            case ARROW_LEFT:
            case ARROW_DOWN:
            case ARROW_UP:
            case ARROW_RIGHT:
                moveCursor(key);
                break;
            }
        }
    }

    void handleVisualLine(int key) {
        if (key == '\x1b') {
            current_mode = Mode::Normal;
        } else {
            handleNormal(key);
            current_mode = Mode::VisualLine;
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

int main(int argc, char *argv[]) {
    try {
        Editor e;
        if (argc >= 2) {
            e.openFile(argv[1]);
        }
        e.run();
    } catch (const std::system_error &e) {
        std::cerr << "Fatal error: " << e.what() << "\r\n" << std::flush;
    }
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
}
