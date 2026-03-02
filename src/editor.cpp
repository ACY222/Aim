#include "editor.hpp"
#include <fstream>
#include <string>
#include <string_view>
#include <unistd.h>
#include <vector>

void Editor::resetCommandState() {
    multiplier = 0;
    pending_operator = '\0';
}

Editor::Editor()
    : cx(0), cy(0), row_off(0), col_off(0), current_mode(Mode::Normal),
      should_quit(false), message("Welcome to Aim!"), yank_by_line(false) {
    if (rows.empty()) {
        rows.push_back("");
    }
}

void Editor::openFile(std::string_view filepath) {
    rows.erase(rows.begin(), rows.end());
    filename = filepath;
    std::ifstream file(filename);

    if (file.is_open()) {
        std::string line;
        while (std::getline(file, line)) {
            if (!line.empty() and line.back() == '\r') {
                line.pop_back();
            }

            rows.push_back(line);
        }
    }

    if (rows.empty()) {
        // fails to open the file
        rows.push_back("");
    }
}

void Editor::run() {
    while (!should_quit) {
        refreshScreen();
        int key = term.readKey();
        processKeyPress(key);
    }
}

void Editor::scroll() {
    if (cy < row_off) { // scroll up
        row_off = cy;
    } else if (cy >= row_off + term.rows - 2) { // scroll down
        row_off = cy - term.rows + 3;
    }

    if (cx < col_off) {
        col_off = cx;
    } else if (cx >= col_off + term.cols - (LINE_NUMBER_LEN + 1)) {
        col_off = cx - term.cols + 1 + (LINE_NUMBER_LEN + 1);
    }
}

void Editor::setCursorStyle(std::string &ab) {
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

void Editor::refreshScreen() {
    scroll();
    std::string ab; // append buffer

    // hide the cursor
    ab += "\x1b[?25l";
    // reposition the cursor to top-left
    ab += "\x1b[H";
    setCursorStyle(ab);

    drawRows(ab);
    drawStatusBar(ab);
    drawMessageBar(ab);

    // move the cursor to (row, col)
    ab += std::format("\x1b[{};{}H", cy - row_off + 1,
                      cx - col_off + 1 + (LINE_NUMBER_LEN + 1));
    // make the cursor visible
    ab += "\x1b[?25h";
    // write to STDOUT_FILENO
    write(STDOUT_FILENO, ab.c_str(), ab.size());
}

void Editor::drawRows(std::string &ab) {
    for (int y = 0; y < term.rows - 2; ++y) {
        int file_row = y + row_off;
        if (file_row < std::ssize(rows)) {
            // use relative line numbers instead of absolute line numbers
            if (file_row == cy) {
                ab += std::format("{:>{}} ", file_row + 1, LINE_NUMBER_LEN);
            } else {
                ab += std::format("{:>{}} ", std::abs(file_row - cy),
                                  LINE_NUMBER_LEN);
            }

            if (col_off < std::ssize(rows[file_row])) {
                ab.append(rows[file_row], col_off,
                          term.cols - LINE_NUMBER_LEN - 1);
            }
        } else {
            if (rows.size() == 0 and y == term.rows / 3) {
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

void Editor::drawStatusBar(std::string &ab) {
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

    std::string display_name = filename.empty() ? "[No Name]" : filename;
    std::string cursor_position =
        std::format("{}:{}  {:>5}", cy + 1, cx + 1, std::ssize(rows));

    int padding_len =
        term.cols - MODE_LEN - cursor_position.size() - FILENAME_LEN;
    std::string padding = padding_len > 0 ? std::string(padding_len, ' ') : "";

    ab += std::format("{:>{}}{:>{}}{}{}", mode, MODE_LEN, display_name,
                      FILENAME_LEN, padding, cursor_position);
    ab += "\x1b[K";
    ab += "\x1b[m"; // reset color
}

void Editor::drawMessageBar(std::string &ab) {
    ab += "\x1b[K";

    if (current_mode == Mode::CommandLine) {
        ab += std::format(":{}", command_buffer);
    } else if (!message.empty()) {
        ab += message;
    }
}

void Editor::processKeyPress(int key) {
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

void Editor::handleNormal(int key) {
    // clear the message to show the keys
    message.clear();

    if (key == '\x1b') {
        resetCommandState();
    }

    if (key >= '1' and key <= '9') {
        multiplier = multiplier * 10 + (key - '0');
        return;
    }

    int count = multiplier == 0 ? 1 : multiplier;

    switch (key) {
    /*** file I/O ***/
    case 'Q':
        should_quit = true;
        break;

    case 'S':
        saveFile();
        break;

    case 'c':
    case 'd':
    case 'y':
        handleOperator(key, count);
        break;

    case 'P':
    case 'p':
        pasteRegister(key, count);
        resetCommandState();
        break;

    /*** change mode ***/
    // Insert mode
    case 'a':
        ++cx;
        current_mode = Mode::Insert;
        break;
    case 'A':
        cx = std::ssize(rows[cy]);
        current_mode = Mode::Insert;
        break;

    case 'I':
        cx = 0;
    case 'i':
        current_mode = Mode::Insert;
        break;

    case 'o':
        ++cy;
    case 'O':
        rows.insert(rows.begin() + cy, "");
        cx = 0;
        current_mode = Mode::Insert;
        break;

    case 's':
        if (rows[cy].empty()) {
            rows[cy].erase(rows[cy].begin() + cx);
        }
        current_mode = Mode::Insert;
        break;

    // case 'S':    // currently, we use `S` to save
    //     rows[cy].erase(cx);
    //     current_mode = Mode::Insert;
    //     break;

    // Visual mode
    case 'v':
    case 'V':
    case CTRL_KEY('v'):
        current_mode = Mode::Visual;
        break;

    // CommandLine mode
    case ':':
        current_mode = Mode::CommandLine;
        message.clear();
        break;

    /*** move operations ***/
    case 'h':
    case 'j':
    case 'k':
    case 'l':
    case ARROW_LEFT:
    case ARROW_DOWN:
    case ARROW_UP:
    case ARROW_RIGHT:
        if (pending_operator) { // operations like d3j
            executeOperatorAction(pending_operator, key, count);
            pending_operator = '\0';
        } else { // operations like 4l
            for (int i = 0; i < count; ++i) {
                moveCursor(key);
            }
        }
        multiplier = 0;
        break;
    case 'G':
        moveCursor(key);
        break;

    case PAGE_UP:
    case CTRL_KEY('b'):
        cy -= term.rows;
        break;
    case PAGE_DOWN:
    case CTRL_KEY('f'):
        cy += term.rows;
        break;

    case CTRL_KEY('u'):
        cy -= term.rows / 2;
        break;
    case CTRL_KEY('d'):
        cy += term.rows / 2;
        break;

    case HOME_KEY:
    case '0':
        if (multiplier) {
            multiplier = multiplier * 10;
        } else {
            cx = 0;
        }
        break;
    case END_KEY:
    case '$':
        cx = rows[cy].empty() ? 0 : rows[cy].size() - 1;
        break;

    /*** edit in normal mode ***/
    case 'J': // merge current line and next line
        if (cy < std::ssize(rows) - 1) {
            auto old_len = std::ssize(rows[cy]);

            if (!rows[cy].empty() and !rows[cy + 1].empty() and
                rows[cy].back() != ' ') {
                rows[cy].push_back(' ');
                ++old_len;
            }
            rows[cy].append(rows[cy + 1]);
            rows.erase(rows.begin() + cy + 1);

            cx = old_len - 1;
        }
        break;
    case 'x': // delete the character at the cursor position
        if (!rows[cy].empty()) {
            rows[cy].erase(rows[cy].begin() + cx);
        }
        break;
    }
    clampCursor();
}

void Editor::handleInsert(int key) {
    switch (key) {
    case '\x1b': // Escape
        current_mode = Mode::Normal;
        if (cx > 0) {
            --cx; // vim standard
        }
        break;

    // expand tab into spaces
    case '\t': { // Tab
        int num_spaces = SOFTTABSTOP - cx % SOFTTABSTOP;
        rows[cy].insert(rows[cy].begin() + cx, num_spaces, ' ');
        cx += num_spaces;
        break;
    }

    case '\r': // Enter
        insertNewLine();
        break;

    case DEL_KEY:
        if (cx < std::ssize(rows[cy])) {
            ++cx;
            deleteChar();
        }
        break;
    case BACKSPACE:
    case CTRL_KEY('h'):
        deleteChar();
        break;

    case ARROW_LEFT:
    case ARROW_DOWN:
    case ARROW_UP:
    case ARROW_RIGHT:
        moveCursor(key);
        break;

    default:
        if (std::isprint(key)) {
            insertChar(key);
        }
    }
}

void Editor::handleVisual(int key) {
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

void Editor::handleVisualLine(int key) {
    if (key == '\x1b') {
        current_mode = Mode::Normal;
    } else {
        handleNormal(key);
        current_mode = Mode::VisualLine;
    }
}

void Editor::handleCommandLine(int key) {
    if (key == '\x1b') {
        current_mode = Mode::Normal;
        command_buffer.clear();
        message.clear();
    } else if (key == '\r') {
        executeCommand();
    } else if (key == BACKSPACE or key == CTRL_KEY('h')) {
        if (command_buffer.empty()) {
            current_mode = Mode::Normal;
        } else {
            command_buffer.pop_back();
        }
    } else if (std::isprint(key)) {
        command_buffer.push_back(static_cast<char>(key));
    }
}

// [count] [motion]
// like 2w, 3j
void Editor::handleOperator(int op, int count) {
    if (pending_operator == '\0') {
        pending_operator = op;
        return;
    } else if (pending_operator == op) {
        // like dd, yy, cc
        executeLineOperator(op, count);
    } // else pending_operator != op_key
    pending_operator = '\0';
}

inline bool Editor::isLineMotion(int motion) {
    return motion == 'j' or motion == ARROW_DOWN or motion == 'k' or
           motion == ARROW_UP;
}

inline bool Editor::isToRight(int motion) {
    return motion == 'l' or motion == ARROW_RIGHT;
}

// [operator] [count] [motion]
// like d2j, c3l, y4k, and d2w, c3e
void Editor::executeOperatorAction(char op, int motion, int count) {
    int coefficient;
    switch (motion) {
    case 'j':
    case ARROW_DOWN:
    case 'l':
    case ARROW_RIGHT:
        coefficient = 1;
        break;
    case 'k':
    case ARROW_UP:
    case 'h':
    case ARROW_LEFT:
        coefficient = -1;
        break;
    }

    if (isLineMotion(motion)) {
        // d2k == kkd2j
        if (coefficient == -1) {
            cy = std::max(cy - count, 0);
        }
        executeLineOperator(op, count + 1);
    } else {
        executeCharsOperator(op, isToRight(motion), count);
    }
}

// d3l
void Editor::executeCharsOperator(int op, int count, bool toRight) {
    yank_register.clear();
    yank_by_line = false;
    int chars_to_affect;
    if (toRight) {
        chars_to_affect =
            std::min(count, static_cast<int>(std::ssize(rows[cy])) - cx);
        yank_register.push_back(rows[cy].substr(cx, chars_to_affect));
        if (op == 'c' or op == 'd') {
            rows[cy].erase(cx, chars_to_affect);
        }
    } else {
        chars_to_affect = std::min(count, cx);
        yank_register.push_back(
            rows[cy].substr(cx - chars_to_affect, chars_to_affect));
        if (op == 'c' or op == 'd') {
            rows[cy].erase(cx - chars_to_affect, chars_to_affect);
        }
        cx -= chars_to_affect;
    }

    if (op == 'c') {
        current_mode = Mode::Insert;
    }
    multiplier = 0;
}

// cc, dd, yy
void Editor::executeLineOperator(int op, int count) {
    int lines_to_affect =
        std::min(count, static_cast<int>(std::ssize(rows) - cy));

    yank_register.clear();
    yank_by_line = true;
    for (int i = 0; i < lines_to_affect; ++i) {
        yank_register.push_back(rows[cy + i]);
    }

    if (op == 'c') {
        for (int i = 0; i < lines_to_affect; ++i) {
            rows[cy + i].clear();
        }
        current_mode = Mode::Insert;
    } else if (op == 'd') {
        rows.erase(rows.begin() + cy, rows.begin() + cy + lines_to_affect);
    }
    multiplier = 0;
}

void Editor::pasteRegister(char op, int count) {
    if (yank_register.empty() or count == 0) {
        return;
    }
    if (yank_by_line) { // then paste by line
        if (op == 'p') {
            ++cy;
            for (int i = 0; i < count; ++i) {
                rows.insert(rows.begin() + cy, yank_register.begin(),
                            yank_register.end());
            }
        } else { // op == 'P'
            for (int i = 0; i < count; ++i) {
                rows.insert(rows.begin() + cy, yank_register.begin(),
                            yank_register.end());
            }
        }
    } else { // paste by characters
        if (op == 'p') {
            for (int i = 0; i < count; ++i) {
                rows[cy].insert(rows[cy].begin() + cx + 1,
                                yank_register[0].begin(),
                                yank_register[0].end());
                cx += std::ssize(yank_register[0]);
            }
        } else { // op == 'P'
            for (int i = 0; i < count; ++i) {
                rows[cy].insert(rows[cy].begin() + cx, yank_register[0].begin(),
                                yank_register[0].end());
            }
            cx = cx + count * std::ssize(yank_register[0]) - 1;
        }
    }
    multiplier = 0;
}

void Editor::executeCommand() {
    if (command_buffer == "q") {
        should_quit = true;
    } else if (command_buffer == "w") {
        saveFile();
    } else if (command_buffer == "wq") {
        saveFile();
        should_quit = true;
    } else {
        message = std::format("Not an editor command yet: {}", command_buffer);
    }

    command_buffer.clear();
    current_mode = Mode::Normal;
}

void Editor::clampCursor() {
    int high = std::ssize(rows[cy]);
    if (high > 0 and current_mode != Mode::Insert) {
        --high;
    }
    cx = std::clamp(cx, 0, high);
    cy = std::clamp<int>(cy, 0, std::ssize(rows) - 1);
}

void Editor::moveCursor(int key) {
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
    case ARROW_RIGHT: {
        int row_size = static_cast<int>(rows[cy].size());
        if (current_mode != Mode::Insert) {
            --row_size;
        }
        if (cx < row_size) {
            ++cx;
        } else if (cy < static_cast<int>(rows.size()) - 1) {
            ++cy;
            cx = 0;
        }
        break;
    }
    case 'G':
        cy = rows.size() - 1;
    }
    clampCursor(); // make sure that cx, cy won't be out of range
}

void Editor::deleteChar() {
    if (cx == 0 and cy == 0) {
        return;
    }

    if (cx > 0) {
        rows[cy].erase(rows[cy].begin() + cx - 1);
        --cx;
    } else {
        moveCursor(ARROW_LEFT);
        rows[cy].append(rows[cy + 1]);
        rows.erase(rows.begin() + cy + 1);
    }
    return;
}

void Editor::insertChar(char c) {
    if (cy == static_cast<int>(rows.size())) {
        rows.push_back("");
    }

    rows[cy].insert(rows[cy].begin() + cx, c);
    ++cx;
}

// call in insert mode
void Editor::insertNewLine() {
    // at the end of the buffer
    if (cy == std::ssize(rows)) {
        rows.emplace_back("");
    } else if (cx == 0) {
        // at the beginning of a line
        rows.insert(rows.begin() + cy, "");
    } else {
        std::string new_row = rows[cy].substr(cx);
        rows[cy].erase(cx);
        rows.insert(rows.begin() + cy + 1, std::move(new_row));
    }

    ++cy;
    cx = 0;
}

void Editor::saveFile() {
    if (filename.empty()) {
        message = "Error! No filename";
        return;
    }

    // std::ios::truc ensures we wipe the existing file clean before writing
    // the new state
    std::ofstream file(filename, std::ios::trunc);

    if (!file.is_open()) {
        message = "Error! Failed to open file";
        return;
    }

    for (const auto &row : rows) {
        file << row << '\n';
    }

    if (file.bad()) {
        message = "Error! I/O failure when writing files";
        return;
    }

    message = std::format("\"{}\" saved!", filename);
}
