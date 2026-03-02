#pragma once
#include <iterator>
#include <string>
#include <vector>

class Buffer {
  private:
    std::vector<std::string> rows;

  public:
    std::string filename;

    Buffer() { rows.emplace_back(""); }

    /*** file operations ***/
    void openFile(std::string_view filepath, std::string &message);
    void saveFile(std::string &message);

    /*** text information ***/
    int getLineCount() const { return std::ssize(rows); }

    int getLineLength(const int cy) const {
        return cy < getLineCount() ? std::ssize(rows[cy]) : 0;
    }

    std::string getLine(int cy) const {
        return cy < getLineCount() ? rows[cy] : "";
    }

    bool isEmpty(const int cy) const {
        return cy < getLineCount() ? rows[cy].empty() : true;
    }

    /*** text editing ***/
    void insertChar(const int cx, const int cy, const char c);
    void appendChar(const int cy, const char c);
    void deleteChar(const int cx, const int cy);
    void deleteChars(const int cx, const int cy, int count);

    void insertString(const int cx, const int cy, const std::string &str);
    void appendString(const int cy, const std::string &str);
    void insertNewLine(const int cx, const int cy);
    void insertLines(const int cy, const std::vector<std::string> &line);

    /*** row operations ***/
    void deleteLines(const int cy, const int count);
    void clearLines(const int cy, const int count);
};
