#include <cctype>
#include <iostream>
#include <system_error>
#include <termios.h>
#include <unistd.h>

class Terminal {
  private:
    struct termios orig_termios;

    void die(const std::string &s) {
        // clear the screen and reposition the cursor when the program dies
        write(STDOUT_FILENO, "\x1b[2J", 4);
        write(STDOUT_FILENO, "\x1b[H", 3);

        throw std::system_error(errno, std::system_category(), s);
    }

  public:
    Terminal() {
        // capture the original terminal attributes into orig_termios
        if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) {
            die("tcgetattr");
        }

        struct termios raw = orig_termios;
        raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
        raw.c_oflag &= ~(OPOST);
        raw.c_cflag |= (CS8);
        raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

        // VMIN: sets the minimum number of bytes of input needed before
        // `read()` can return
        // VTIME: sets the maximum amount of time to wait before `read()`
        // returns, which is in tenths of a second (100ms)
        raw.c_cc[VMIN] = 0;
        raw.c_cc[VTIME] = 1;

        if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
            die("tcsetattr");
        }
    }

    ~Terminal() {
        if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1) {
            die("tcsetattr");
        }
    }

    int readKey() {
        int nread;
        char c;
        while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
            if (nread == -1 and errno != EAGAIN) {
                die("read");
            }
        }
        return c;
    }
};

int main() {
    try {
        Terminal term;
        char c;
        while (true) {
            c = term.readKey();

            if (std::iscntrl(c)) {
                std::cout << static_cast<int>(c) << "\r\n" << std::flush;
            } else {
                std::cout << static_cast<int>(c) << "(" << c << ")\r\n"
                          << std::flush;
            }

            if (c == 'q') {
                break;
            }
        }
    } catch (const std::system_error &e) {
        std::cerr << "Fatal error: " << e.what() << "\r\n" << std::flush;
    }
}
