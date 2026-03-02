#pragma once
#include <iterator>
#include <string>
#include <vector>

class Buffer {
  public:
    std::string filename;
    std::vector<std::string> rows;

    Buffer() { rows.emplace_back(""); }

    /*** file operations ***/
    void openFile(std::string_view filepath, std::string &message);
    void saveFile(std::string &message);

    /*** text information ***/
    int getLineCount() const { return std::ssize(rows); }

    int getLineLength(const int cy) const {
        return cy < getLineCount() ? std::ssize(rows[cy]) : 0;
    }

    bool isEmpty(const int cy) const {
        return cy < getLineCount() ? rows[cy].empty() : true;
    }

    /*** text editing ***/
    void insertChar(int &cx, const int cy, const char c);
    void deleteChar(int &cx, int &cy);
    void insertString(int &cx, int cy, const std::string &str);

    void insertNewLine(int &cx, int &cy);
    void insertLines(int cy, const std::vector<std::string> &line);

    /*** row operations ***/
    void deleteLines(int &cy, const int count);
    void clearLines(const int cy, const int count);
};
