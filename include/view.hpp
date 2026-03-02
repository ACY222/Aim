#pragma once
#include "buffer.hpp"
#include "terminal.hpp"

class View {
  private:
    Terminal &term;
    int row_off{0}, col_off{0};

    void setCursorStyle(std::string &ab, Mode mode) const;
    void drawRows(std::string &ab, const Buffer &buffer, int cy) const;
    void drawStatusBar(std::string &ab, const Buffer &buffer, Mode mode, int cx,
                       int cy) const;
    void drawMessageBar(std::string &ab, Mode mode, const std::string &message,
                        const std::string &command_buffer) const;

  public:
    View(Terminal &t) : term(t) {}

    void scroll(int cx, int cy);
    void refreshScreen(const Buffer &buffer, Mode mode, int cx, int cy,
                       const std::string &message,
                       const std::string &command_buffer);
};
