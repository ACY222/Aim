#include "view.hpp"
#include "config.hpp"
#include <format>
#include <unistd.h>

void View::setCursorStyle(std::string &ab, Mode mode) const {
    switch (mode) {
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

void View::drawRows(std::string &ab, const Buffer &buffer, int cy) const {
    for (int y = 0; y < term.rows - 2; ++y) {
        int file_row = y + row_off;
        if (file_row < buffer.getLineCount()) {
            if (file_row == cy) {
                ab += std::format("{:>{}} ", file_row + 1, LINE_NUMBER_LEN);
            } else {
                ab += std::format("{:>{}} ", std::abs(file_row - cy),
                                  LINE_NUMBER_LEN);
            }

            if (col_off < buffer.getLineLength(file_row)) {
                ab.append(buffer.getLine(file_row), col_off,
                          term.cols - (LINE_NUMBER_LEN + 1));
            }
        } else {
            // if we open an empty file
            if (buffer.filename.empty() and buffer.getLineCount() == 1 and
                buffer.isEmpty(0) and y == term.rows / 3) {
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

void View::drawStatusBar(std::string &ab, const Buffer &buffer, Mode mode,
                         int cx, int cy) const {
    ab += "\x1b[7m"; // reverse color

    std::string mode_str;
    switch (mode) {
    case Mode::Normal:
        mode_str = "Normal";
        break;
    case Mode::Insert:
        mode_str = "Insert";
        break;
    case Mode::Visual:
        mode_str = "Visual";
        break;
    case Mode::VisualLine:
        mode_str = "VisualLine";
        break;
    case Mode::CommandLine:
        mode_str = "Command";
        break;
    }

    std::string display_name =
        buffer.filename.empty() ? "[No Name]" : buffer.filename;
    std::string cursor_position =
        std::format("{}:{}  {:>5}", cy + 1, cx + 1, buffer.getLineCount());

    int padding_len =
        term.cols - MODE_LEN - cursor_position.size() - FILENAME_LEN;
    std::string padding = padding_len > 0 ? std::string(padding_len, ' ') : "";

    ab += std::format("{:>{}}{:>{}}{}{}", mode_str, MODE_LEN, display_name,
                      FILENAME_LEN, padding, cursor_position);
    ab += "\x1b[K";
    ab += "\x1b[m"; // reset color
    ab += "\r\n";
}

void View::drawMessageBar(std::string &ab, Mode mode,
                          const std::string &message,
                          const std::string &command_buffer) const {
    ab += "\x1b[K";

    if (mode == Mode::CommandLine) {
        ab += std::format(":{}", command_buffer);
    } else if (!message.empty()) {
        ab += message;
    }
}

void View::scroll(int cx, int cy) {
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

void View::refreshScreen(const Buffer &buffer, Mode mode, int cx, int cy,
                         const std::string &message,
                         const std::string &command_buffer) {
    scroll(cx, cy);
    std::string ab; // append buffer

    // hide the cursor
    ab += "\x1b[?25l";
    // reposition the cursor to top-left
    ab += "\x1b[H";
    setCursorStyle(ab, mode);

    drawRows(ab, buffer, cy);
    drawStatusBar(ab, buffer, mode, cx, cy);
    drawMessageBar(ab, mode, message, command_buffer);

    if (mode == Mode::CommandLine) {
        ab += std::format("\x1b[{};{}H", term.rows,
                          std::ssize(command_buffer) + 2);
    } else { // move the cursor to (row, col)
        ab += std::format("\x1b[{};{}H", cy - row_off + 1,
                          cx - col_off + 1 + (LINE_NUMBER_LEN + 1));
    }
    // make the cursor visible
    ab += "\x1b[?25h";
    // write to STDOUT_FILENO
    write(STDOUT_FILENO, ab.c_str(), ab.size());
}
