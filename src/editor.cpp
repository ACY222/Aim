#include "editor.hpp"
#include "config.hpp"
#include <cctype>
#include <format>
#include <iterator>
#include <string>
#include <unistd.h>
#include <vector>

void Editor::resetCommandState() {
    multiplier = 0;
    pending_operator = '\0';
}

void Editor::run() {
    resetCommandState();
    while (!should_quit) {
        view.refreshScreen(buffer, mode, cx, cy, message, command_buffer,
                           multiplier, pending_operator);
        int key = term.readKey();
        processKeyPress(key);
    }
}

void Editor::processKeyPress(int key) {
    switch (mode) {
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
    case Mode::Search:
        handleSearch(key);
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
    /*** quit or save ***/
    case 'Q':
        should_quit = true;
        break;

    case 'S':
        if (buffer.filename.empty()) {
            mode = Mode::CommandLine;
            command_buffer = "w ";
            message.clear();
        } else {
            buffer.saveFile(message);
        }
        break;

    /*** editing ***/
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
        mode = Mode::Insert;
        break;
    case 'A':
        cx = buffer.getLineLength(cy);
        mode = Mode::Insert;
        break;

    case 'I':
        cx = 0;
    case 'i':
        mode = Mode::Insert;
        break;

    case 'o':
    case 'O': {
        std::string line{buffer.getLine(cy)};
        int leading_spaces_len = line.find_first_not_of(' ');
        if (key == 'o') {
            ++cy;
        }
        buffer.insertNewLine(0, cy);
        buffer.appendString(cy, std::string(leading_spaces_len, ' '));
        cx = leading_spaces_len;
        mode = Mode::Insert;
        break;
    }

    case 's':
        if (!buffer.isEmpty(cy)) {
            buffer.deleteChar(cx, cy);
        }
        mode = Mode::Insert;
        break;

    // Visual mode
    case 'v':
    case 'V':
    case CTRL_KEY('v'):
        mode = Mode::Visual;
        break;

    // CommandLine mode
    case ':':
        mode = Mode::CommandLine;
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

    case 'w':
    case 'W':
    case 'e':
    case 'E':
    case 'b':
    case 'B':
        if (pending_operator) {
            executeOperatorAction(pending_operator, key, count);
            pending_operator = '\0';
        } else {
            for (int i = 0; i < count; ++i) {
                moveByWord(key);
            }
            multiplier = 0;
        }
        break;

    case 'g':
        handleOperator(key, count);
        break;
    case 'G':
        if (count != 1) {
            cy = count - 1;
            multiplier = 0;
        } else {
            cy = buffer.getLineCount() - 1;
        }
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
        cx = buffer.isEmpty(cy) ? 0 : buffer.getLineLength(cy) - 1;
        break;

    /*** search ***/
    case 'f':
    case 't':
    case 'F':
    case 'T': {
        int target_char = term.readKey();
        if (target_char != '\x1b') {
            executeLineSearch(key, target_char, count);
        }
        multiplier = 0;
        break;
    }

    case '?':
    case '/': {
        mode = Mode::Search;
        command_buffer.clear();
        command_buffer.push_back(key);
        message.clear();
        break;
    }

    case 'n':
        executeFileSearch(1);
        break;
    case 'N':
        executeFileSearch(-1);
        break;

    /*** edit in normal mode ***/
    case 'J': // merge current line and next line
        if (cy < buffer.getLineCount() - 1) {
            auto old_len = buffer.getLineLength(cy);

            if (!buffer.isEmpty(cy) and !buffer.isEmpty(cy + 1)) {
                buffer.appendChar(cy, ' ');
                ++old_len;
            }
            buffer.appendString(cy, buffer.getLine(cy + 1));
            buffer.deleteLines(cy + 1, 1);

            cx = old_len - 1;
        }
        break;
    case 'x': // delete the character at the cursor position
        if (!buffer.isEmpty(cy)) {
            buffer.deleteChar(cx, cy);
        }
        break;
    }
    clampCursor();
}

void Editor::handleInsert(int key) {
    switch (key) {
    case '\x1b': // Escape
        mode = Mode::Normal;
        if (cx > 0) {
            --cx; // vim standard
        }
        break;

    // expand tab into spaces
    case '\t': { // Tab
        int num_spaces = SOFTTABSTOP - cx % SOFTTABSTOP;
        for (int i = 0; i < num_spaces; ++i) {
            buffer.insertChar(cx, cy, ' ');
        }
        cx += num_spaces;
        break;
    }

    case '\r': { // Enter
        std::string line = buffer.getLine(cy);
        auto leading_spaces_len = line.find_first_not_of(' ');
        buffer.insertNewLine(cx, cy);
        ++cy;
        if (leading_spaces_len != std::string::npos) {
            buffer.appendString(cy, std::string(leading_spaces_len, ' '));
            cx = leading_spaces_len;
        }
        break;
    }

    case DEL_KEY:
        if (cx < buffer.getLineLength(cy)) {
            buffer.deleteChar(cx + 1, cy);
        }
        break;
    case BACKSPACE:
    case CTRL_KEY('h'):
        if (cx == 0 and cy == 0) {
            break;
        }
        if (cx > 0) {
            buffer.deleteChar(cx - 1, cy);
            --cx;
        } else { // cx == 0
            cx = buffer.getLineLength(cy - 1);
            buffer.appendString(cy - 1, buffer.getLine(cy));
            buffer.deleteLines(cy, 1);
            --cy;
        }
        break;

    case ARROW_LEFT:
    case ARROW_DOWN:
    case ARROW_UP:
    case ARROW_RIGHT:
        moveCursor(key);
        break;

    default:
        if (std::isprint(key)) {
            buffer.insertChar(cx, cy, key);
            ++cx;
        }
    }
}

void Editor::handleVisual(int key) {
    if (key == '\x1b') {
        mode = Mode::Normal;
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
        mode = Mode::Normal;
    } else {
        handleNormal(key);
        mode = Mode::VisualLine;
    }
}

void Editor::handleCommandLine(int key) {
    if (key == '\x1b') {
        mode = Mode::Normal;
        command_buffer.clear();
        message.clear();
    } else if (key == '\r') {
        executeCommand();
    } else if (key == BACKSPACE or key == CTRL_KEY('h')) {
        if (command_buffer.empty()) {
            mode = Mode::Normal;
        } else {
            command_buffer.pop_back();
        }
    } else if (std::isprint(key)) {
        command_buffer.push_back(static_cast<char>(key));
    }
}

void Editor::handleSearch(int key) {
    if (key == '\x1b') {
        mode = Mode::Normal;
        command_buffer.clear();
    } else if (key == '\r') {
        int dir = (command_buffer[0] == '/') ? 1 : -1;
        last_search_query = command_buffer.substr(1);
        command_buffer.clear();
        mode = Mode::Normal;
        executeFileSearch(dir);
    } else if (key == BACKSPACE or key == CTRL_KEY('h')) {
        if (command_buffer.size() <= 1) {
            mode = Mode::Normal;
            command_buffer.clear();
        } else {
            command_buffer.pop_back();
        }
    } else if (std::isprint(key)) {
        command_buffer.push_back(key);
    }
}

// [count] [motion]
// like 2w, 3j
void Editor::handleOperator(int op, int count) {
    if (pending_operator == '\0') {
        pending_operator = op;
        return;
    } else if (pending_operator == op) {
        if (op == 'g') {
            cy = count - 1;
            multiplier = 0;
        } else {
            // like dd, yy, cc
            executeLineOperator(op, count);
        }
    } // else pending_operator != op_key
    pending_operator = '\0';
}

inline bool Editor::moveVertically(int motion) {
    return motion == 'j' or motion == ARROW_DOWN or motion == 'k' or
           motion == ARROW_UP;
}

inline bool Editor::moveHorizontally(int motion) {
    return motion == 'h' or motion == ARROW_LEFT or motion == 'l' or
           motion == ARROW_RIGHT;
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

    if (moveVertically(motion)) {
        // d2k == kkd2j
        if (coefficient == -1) {
            cy = std::max(cy - count, 0);
        }
        executeLineOperator(op, count + 1);
    } else if (moveHorizontally(motion)) {
        executeCharsOperator(op, count, isToRight(motion));
    } else { // motion == web or WEB
        int orig_cx = cx, orig_cy = cy;
        for (int i = 0; i < count; ++i) {
            moveByWord(motion);
        }
        multiplier = 0;
        if (cy != orig_cy) { // move out of line
            if (motion == 'b' or motion == 'B') {
                cx = 0;
                cy = orig_cy;
                // if (motion == 'b') {
                //     moveWordBackward();
                // } else {
                //     moveWORDBackward();
                // }
                buffer.deleteChars(cx, cy, orig_cx);
            } else { // motion == we or WE
                cx = orig_cx;
                cy = orig_cy;
                // switch (motion) {
                // case 'w':
                //     moveWordForward();
                //     break;
                // case 'W':
                //     moveWORDForward();
                //     break;
                // case 'e':
                //     moveWordEndForward();
                //     break;
                // case 'E':
                //     moveWORDEndForward();
                //     break;
                // }
                buffer.deleteChars(cx, cy, buffer.getLineLength(cy) - cx - 1);
            }
        } else { // still in current line
            if (motion == 'b' or motion == 'B') {
                buffer.deleteChars(cx, cy, orig_cx - cx);
            } else if (motion == 'w' or motion == 'W') {
                buffer.deleteChars(orig_cx, cy, cx - orig_cx);
                cx = orig_cx;
            } else { // motion == 'e' or 'E'
                buffer.deleteChars(orig_cx, cy, cx - orig_cx + 1);
                cx = orig_cx;
            }
        }
    }
}

// d3l
void Editor::executeCharsOperator(int op, int count, bool toRight) {
    yank_register.clear();
    yank_by_line = false;
    int chars_to_affect;
    if (toRight) {
        chars_to_affect =
            std::min(count, static_cast<int>(buffer.getLineLength(cy)) - cx);
        yank_register.push_back(buffer.getLine(cy).substr(cx, chars_to_affect));
        if (op == 'c' or op == 'd') {
            buffer.deleteChars(cx, cy, chars_to_affect);
        }
    } else {
        chars_to_affect = std::min(count, cx);
        yank_register.push_back(
            buffer.getLine(cy).substr(cx - chars_to_affect, chars_to_affect));
        if (op == 'c' or op == 'd') {
            buffer.deleteChars(cx - chars_to_affect, cy, chars_to_affect);
        }
        cx -= chars_to_affect;
    }

    if (op == 'c') {
        mode = Mode::Insert;
    }
    multiplier = 0;
}

// cc, dd, yy
void Editor::executeLineOperator(int op, int count) {
    int lines_to_affect =
        std::min(count, static_cast<int>(buffer.getLineCount() - cy));

    yank_register.clear();
    yank_by_line = true;
    for (int i = 0; i < lines_to_affect; ++i) {
        yank_register.push_back(buffer.getLine(cy + i));
    }

    if (op == 'c') {
        buffer.clearLines(cy, count);
        mode = Mode::Insert;
    } else if (op == 'd') {
        buffer.deleteLines(cy, count);
    }
    multiplier = 0;
}

void Editor::pasteRegister(char op, int count) {
    if (yank_register.empty() or count == 0) {
        return;
    }
    if (yank_by_line) { // then paste by line
        int insert_cy = (op == 'p') ? cy + 1 : cy;
        for (int i = 0; i < count; ++i) {
            buffer.insertLines(insert_cy, yank_register);
            insert_cy += std::ssize(yank_register);
        }

        if (op == 'p') {
            ++cy;
        }
    } else { // paste by characters
        std::string to_paste = "";
        for (int i = 0; i < count; ++i) {
            to_paste += yank_register[0];
        }
        if (op == 'p') {
            if (buffer.isEmpty(cy)) {
                buffer.insertString(cx, cy, yank_register[0]);
            } else {
                buffer.insertString(cx + 1, cy, yank_register[0]);
            }
            cx = cx + std::size(yank_register[0]);
        } else { // op == 'P'
            buffer.insertString(cx, cy, yank_register[0]);
            cx = cx + std::size(yank_register[0]) - 1;
        }
    }
    multiplier = 0;
}

void Editor::executeCommand() {
    if (command_buffer == "q") {
        should_quit = true;
    } else if (command_buffer.starts_with('w') and command_buffer != "wq") {
        if (command_buffer.size() >= 3) {
            std::string new_filename = command_buffer.substr(2);
            if (!new_filename.empty()) {
                buffer.filename = new_filename;
            }
        }
        buffer.saveFile(message);
    } else if (command_buffer == "wq") {
        buffer.saveFile(message);
        should_quit = true;
    } else {
        message = std::format("Not an editor command yet: {}", command_buffer);
    }

    command_buffer.clear();
    mode = Mode::Normal;
}

void Editor::clampCursor() {
    int high = buffer.getLineLength(cy);
    if (high > 0 and mode != Mode::Insert) {
        --high;
    }
    cx = std::clamp(cx, 0, high);
    cy = std::clamp<int>(cy, 0, buffer.getLineCount() - 1);
}

void Editor::moveCursor(int key) {
    switch (key) {
    case 'h':
    case ARROW_LEFT:
        if (cx > 0) {
            --cx;
        } else if (cy > 0) {
            --cy;
            cx = buffer.getLineLength(cy);
            if (mode == Mode::Normal) {
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
        int row_size = buffer.getLineLength(cy);
        if (mode != Mode::Insert) {
            --row_size;
        }
        if (cx < row_size) {
            ++cx;
        } else if (cy < buffer.getLineCount() - 1) {
            ++cy;
            cx = 0;
        }
        break;
    }
    }
    clampCursor(); // make sure that cx, cy won't be out of range
}

CharType Editor::getCharType(char c) const {
    if (std::isspace(c)) {
        return CharType::Space;
    } else if (std::isalnum(c) or c == '_') {
        return CharType::Word;
    } else {
        return CharType::Punctuation;
    }
}

void Editor::moveByWord(int motion) {
    switch (motion) {
    case 'w':
        moveWordForward();
        break;
    case 'W':
        moveWORDForward();
        break;
    case 'e':
        moveWordEndForward();
        break;
    case 'E':
        moveWORDEndForward();
        break;
    case 'b':
        moveWordBackward();
        break;
    case 'B':
        moveWORDBackward();
        break;
    }
}

void Editor::moveWordForward() {
    std::string line = buffer.getLine(cy);
    int len = buffer.getLineLength(cy);

    CharType type = getCharType(line[cx]);

    // 1. skip current word block
    while (cx < len and getCharType(line[cx]) == type and
           type != CharType::Space) {
        ++cx;
    }

    // 2. skip trailing space
    while (cx < len and getCharType(line[cx]) == CharType::Space) {
        ++cx;
    }

    // 3. wrap to next line if we hit the end
    if (cx >= len and cy < buffer.getLineCount() - 1) {
        ++cy;
        cx = 0;
        line = buffer.getLine(cy);
        len = buffer.getLineLength(cy);
        // skip the leading space
        while (cx < len and getCharType(line[cx]) == CharType::Space) {
            ++cx;
        }
    } else if (cx >= len and len > 0) {
        cx = len - 1;
    }
}

void Editor::moveWordEndForward() {
    std::string line = buffer.getLine(cy);
    int len = buffer.getLineLength(cy);

    // move one character forward
    if (cx >= len - 1) {
        if (cy < buffer.getLineCount() - 1) {
            ++cy;
            cx = 0;
            line = buffer.getLine(cy);
            len = buffer.getLineLength(cy);
        } else {
            return;
        }
    } else {
        ++cx;
    }

    // skip spaces
    while (cx < len and getCharType(line[cx]) == CharType::Space) {
        ++cx;
        if (cx >= len and cy < buffer.getLineCount() - 1) {
            ++cy;
            cx = 0;
            line = buffer.getLine(cy);
            len = buffer.getLineLength(cy);
        }
    }

    if (cx >= len) {
        cx = (len > 0) ? len - 1 : 0;
        return;
    }

    // stops at the last character of the current block
    CharType type = getCharType(line[cx]);
    while (cx < len - 1 and getCharType(line[cx + 1]) == type) {
        ++cx;
    }
}

void Editor::moveWordBackward() {
    std::string line = buffer.getLine(cy);

    // move one character backward
    if (cx <= 0) {
        if (cy > 0) {
            --cy;
            line = buffer.getLine(cy);
            cx = buffer.getLineLength(cy) - 1;
        } else {
            return;
        }
    } else {
        --cx;
    }

    // skip spaces
    while (cx >= 0 and getCharType(line[cx]) == CharType::Space) {
        --cx;
        if (cx <= 0 and cy > 0) {
            --cy;
            line = buffer.getLine(cy);
            cx = buffer.getLineLength(cy) - 1;
        }
    }

    CharType type = getCharType(line[cx]);
    while (cx > 0 and getCharType(line[cx - 1]) == type) {
        --cx;
    }
}

void Editor::moveWORDForward() {
    std::string line = buffer.getLine(cy);
    int len = buffer.getLineLength(cy);

    CharType type = getCharType(line[cx]);

    // 1. skip current word block
    while (cx < len and getCharType(line[cx]) != CharType::Space and
           type != CharType::Space) {
        ++cx;
    }

    // 2. skip trailing space
    while (cx < len and getCharType(line[cx]) == CharType::Space) {
        ++cx;
    }

    // 3. wrap to next line if we hit the end
    if (cx >= len and cy < buffer.getLineCount() - 1) {
        ++cy;
        cx = 0;
        line = buffer.getLine(cy);
        len = buffer.getLineLength(cy);
        // skip the leading space
        while (cx < len and getCharType(line[cx]) == CharType::Space) {
            ++cx;
        }
    } else if (cx >= len and len > 0) {
        cx = len - 1;
    }
}
void Editor::moveWORDEndForward() {
    std::string line = buffer.getLine(cy);
    int len = buffer.getLineLength(cy);

    // move one character forward
    if (cx >= len - 1) {
        if (cy < buffer.getLineCount() - 1) {
            ++cy;
            cx = 0;
            line = buffer.getLine(cy);
            len = buffer.getLineLength(cy);
        } else {
            return;
        }
    } else {
        ++cx;
    }

    // skip spaces
    while (cx < len and getCharType(line[cx]) == CharType::Space) {
        ++cx;
        if (cx >= len and cy < buffer.getLineCount() - 1) {
            ++cy;
            cx = 0;
            line = buffer.getLine(cy);
            len = buffer.getLineLength(cy);
        }
    }

    if (cx >= len) {
        cx = (len > 0) ? len - 1 : 0;
        return;
    }

    // stops at the last character of the current block
    while (cx < len - 1 and getCharType(line[cx + 1]) != CharType::Space) {
        ++cx;
    }
}

void Editor::moveWORDBackward() {
    std::string line = buffer.getLine(cy);

    // move one character backward
    if (cx <= 0) {
        if (cy > 0) {
            --cy;
            line = buffer.getLine(cy);
            cx = buffer.getLineLength(cy) - 1;
        } else {
            return;
        }
    } else {
        --cx;
    }

    // skip spaces
    while (cx >= 0 and getCharType(line[cx]) == CharType::Space) {
        --cx;
        if (cx <= 0 and cy > 0) {
            --cy;
            line = buffer.getLine(cy);
            cx = buffer.getLineLength(cy) - 1;
        }
    }

    while (cx > 0 and getCharType(line[cx - 1]) != CharType::Space) {
        --cx;
    }
}

void Editor::executeFileSearch(int dir) {
    if (last_search_query.empty()) {
        return;
    }

    int current_cy = cy;
    int current_cx = cx + dir;
    while (true) {
        if (current_cy >= 0 and current_cy < buffer.getLineCount()) {
            std::string line = buffer.getLine(current_cy);
            int match_idx = -1;

            if (dir == 1) { // search forawrd
                size_t search_start = (current_cy == cy) ? current_cx : 0;
                size_t pos = line.find(last_search_query, search_start);
                if (pos != std::string::npos) {
                    match_idx = pos;
                }
            } else { // search backward
                size_t search_start =
                    (current_cy == cy) ? current_cx : std::string::npos;
                size_t pos = line.rfind(last_search_query, search_start);
                if (pos != std::string::npos) {
                    match_idx = pos;
                }
            }

            if (match_idx != -1) {
                cx = match_idx;
                cy = current_cy;
                return;
            }
        }

        current_cy += dir;
        if (current_cy >= buffer.getLineCount()) {
            current_cy = 0;
        } else if (current_cy < 0) {
            current_cy = buffer.getLineCount() - 1;
        }

        if (current_cy == cy) {
            message = std::format("Pattern not found: {}", last_search_query);
            break;
        }
    }
}

void Editor::executeLineSearch(int command, int target_char, int count) {
    std::string line = buffer.getLine(cy);
    if (line.empty()) {
        return;
    }

    int step = (command == 'f' or command == 't') ? 1 : -1;
    int current_cx = cx;

    for (int i = 0; i < count; ++i) {
        int next_cx = current_cx + step;
        bool found = false;

        while (next_cx >= 0 and next_cx < buffer.getLineLength(cy)) {
            if (line[next_cx] == target_char) {
                found = true;
                current_cx = next_cx;
                break;
            }
            next_cx += step;
        }

        if (!found) {
            return;
        }
    }

    if (command == 't') {
        cx = current_cx - 1;
    } else if (command == 'T') {
        cx = current_cx + 1;
    } else {
        cx = current_cx;
    }
}
