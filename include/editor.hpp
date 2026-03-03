#pragma once

#include "buffer.hpp"
#include "config.hpp"
#include "terminal.hpp"
#include "view.hpp"
#include <string>
#include <vector>

class Editor {
  private:
    Terminal term;
    Buffer buffer;
    View view;

    int cx{0}, cy{0};
    Mode mode{Mode::Normal};
    bool should_quit{false};

    // --- Vim State Machine ---
    int multiplier{0};                      // stores the [count]
    char pending_operator{'\0'};            // stores 'd', 'y', 'c', etc.
    std::vector<std::string> yank_register; // stores copied/deleted lines
    bool yank_by_line{false};

    std::string command_buffer;
    std::string message;

    void resetCommandState();

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

    // --- commands in CommandLine mode ---
    void executeCommand();

  public:
    Editor() : view(term) {}

    void openFile(std::string_view filepath) {
        buffer.openFile(filepath, message);
    }

    void run();
};
