#pragma once

#include "config.hpp"
#include "terminal.hpp"
#include <string>
#include <vector>

class Editor {
  private:
    Terminal term;
    // cursor position
    int cx, cy;
    // the first row or column printed in the screen
    int row_off, col_off;

    std::string filename;
    std::vector<std::string> rows;

    Mode current_mode;
    bool should_quit;
    std::string command_buffer;
    std::string message;

    // --- Vim Grammar State ---
    int multiplier;        // stores the [count]
    char pending_operator; // stores 'd', 'y', 'c', etc.

    // --- Clipboard (register) ---
    std::vector<std::string> yank_register; // stores copied/deleted lines
    bool yank_by_line;

    void resetCommandState();
    void saveFile();
    void scroll();
    void setCursorStyle(std::string &ab);
    void refreshScreen();
    void drawRows(std::string &ab);
    void drawStatusBar(std::string &ab);
    void drawMessageBar(std::string &ab);

    // --- process keypress ---
    void processKeyPress(int key);
    void handleNormal(int key);
    void handleInsert(int key);
    void handleVisual(int key);
    void handleVisualLine(int key);
    void handleCommandLine(int key);

    // --- cursor movement
    void moveCursor(int key);
    void clampCursor();

    // --- auxiliary methods ---
    inline bool isLineMotion(int motion);
    inline bool isToRight(int motion);
    // --- commands in Normal mode ---
    // [count] [motion] * 2
    void handleOperator(int op, int count); // 2dd
    // [motion] * 2
    void executeLineOperator(int op, int count); // dd
    // [operator] [count] [motion]
    void executeOperatorAction(char op, int count, int motion); // d2j
    // [operator] [count] [h/l]
    void executeCharsOperator(int op, int count, bool toRight); // d2l
    // [count] p/P
    void pasteRegister(char op, int count); // 2p

    // --- editing in insert mode
    void deleteChar();
    void insertChar(char c);
    void insertNewLine();

    // --- commands in CommandLine mode ---
    void executeCommand();

  public:
    Editor();
    void openFile(std::string_view filepath);
    void run();
};
