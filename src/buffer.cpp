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
void Buffer::insertChar(int &cx, const int cy, const char c) {
    if (cy == getLineCount()) {
        rows.emplace_back("");
    }
    rows[cy].insert(rows[cy].begin() + cx, c);
    ++cx;
}

void Buffer::deleteChar(int &cx, int &cy) {
    if (cx == 0 and cy == 0) {
        return;
    }

    if (cx > 0) {
        rows[cy].erase(rows[cy].begin() + cx - 1);
        --cx;
    } else {
        cx = rows[cy - 1].size();
        rows[cy - 1].append(rows[cy]);
        rows.erase(rows.begin() + cy);
        --cy;
    }
}

void Buffer::insertNewLine(int &cx, int &cy) {
    if (cy == getLineCount()) { // just in case cy is out of range
        rows.emplace_back("");
    } else if (cx == 0) {
        rows.insert(rows.begin() + cy, "");
        ++cy;
    } else {
        std::string new_row = rows[cy].substr(cx);
        rows[cy].erase(cx);
        rows.insert(rows.begin() + cy + 1, std::move(new_row));
        ++cy;
    }

    cx = 0;
}

/*** row operations ***/
void Buffer::deleteLines(int &cy, const int count) {
    if (cy >= getLineCount()) {
        return;
    }

    int lines_to_affect = std::min(count, getLineCount() - cy);
    rows.erase(rows.begin() + cy, rows.begin() + cy + lines_to_affect);

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
