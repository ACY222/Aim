#include "buffer.hpp"
#include <algorithm>
#include <format>
#include <fstream>

void Buffer::openFile(std::string_view filepath, std::string &message) {
    rows.clear();
    filename = filepath;
    std::ifstream file(filename);

    if (!file.is_open()) {
        message = "Error! Fail to open file";
        rows.emplace_back("");
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() and line.back() == '\r') {
            line.pop_back();
        }
        rows.push_back(line);
    }

    if (rows.empty()) {
        rows.emplace_back("");
    }
}

void Buffer::saveFile(std::string &message) {
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

/*** text editing ***/
void Buffer::insertChar(const int cx, const int cy, const char c) {
    if (cy == getLineCount()) {
        rows.emplace_back("");
    }
    rows[cy].insert(rows[cy].begin() + cx, c);
}

void Buffer::appendChar(const int cy, const char c) {
    if (cy == getLineCount()) {
        rows.emplace_back("");
    }
    rows[cy].push_back(c);
}

void Buffer::deleteChar(const int cx, const int cy) {
    if (cy < 0 or cy > getLineCount() or cx < 0 or cx >= getLineLength(cy)) {
        return; // cx or cy is out of range
    }

    if (cx < getLineLength(cy) and cx >= 0) {
        rows[cy].erase(rows[cy].begin() + cx);
    }
}

void Buffer::deleteChars(const int cx, const int cy, const int count) {
    if (cy >= getLineCount()) {
        return;
    }

    int chars_to_del = std::min(count, getLineLength(cy) - cx);
    if (chars_to_del > 0) {
        rows[cy].erase(cx, chars_to_del);
    }
}

void Buffer::insertString(const int cx, const int cy, const std::string &str) {
    if (cy == getLineCount()) {
        rows.emplace_back("");
    }

    rows[cy].insert(rows[cy].begin() + cx, str.begin(), str.end());
}

void Buffer::appendString(const int cy, const std::string &str) {
    if (cy == getLineCount()) {
        rows.emplace_back("");
    }

    rows[cy].append(str);
}

void Buffer::insertNewLine(const int cx, const int cy) {
    if (cy == getLineCount()) { // just in case cy is out of range
        rows.emplace_back("");
    } else if (cx == 0) {
        rows.insert(rows.begin() + cy, "");
    } else {
        std::string new_row = rows[cy].substr(cx);
        rows[cy].erase(cx);
        rows.insert(rows.begin() + cy + 1, std::move(new_row));
    }
}

void Buffer::insertLines(const int cy, const std::vector<std::string> &lines) {
    rows.insert(rows.begin() + cy, lines.begin(), lines.end());
}

/*** row operations ***/
void Buffer::deleteLines(const int cy, const int count) {
    if (cy >= getLineCount()) {
        return;
    }

    int lines_to_affect = std::min(count, getLineCount() - cy);
    if (lines_to_affect > 0) {
        rows.erase(rows.begin() + cy, rows.begin() + cy + lines_to_affect);
    }

    if (rows.empty()) {
        rows.emplace_back("");
    }
}

void Buffer::clearLines(const int cy, const int count) {
    if (cy >= getLineCount()) {
        return;
    }

    int lines_to_affect = std::min(count, getLineCount() - cy);
    if (lines_to_affect > 1) {
        rows.erase(rows.begin() + cy + 1, rows.begin() + cy + lines_to_affect);
    }

    rows[cy].clear();
}
