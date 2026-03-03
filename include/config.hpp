#pragma once

#define CTRL_KEY(k) ((k) & 0x1f)

// the width of line numbers
constexpr int LINE_NUMBER_LEN = 5;
// the width opf Mode and filename in status bar
constexpr int MODE_LEN = 8;
constexpr int FILENAME_LEN = 16;

// how many spaces a Tab keystroke counts for
constexpr int SOFTTABSTOP = 4;

enum class CharType {
    Space,
    Word,
    Punctuation,
};

enum class Mode {
    Normal,
    Insert,
    Visual,
    VisualLine,
    CommandLine,
    Search,
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

struct WindowSize {
    int rows;
    int cols;
};
