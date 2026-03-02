#pragma once

#include "config.hpp"
#include <string>
#include <termios.h>

class Terminal {
  private:
    struct termios orig_termios;

    void die(const std::string &s);
    std::optional<WindowSize> getCursorPosition();
    std::optional<WindowSize> getWindowSize();

  public:
    int rows, cols;

    Terminal();
    ~Terminal();
    int readKey();
};
