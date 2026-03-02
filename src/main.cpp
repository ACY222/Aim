#include "editor.hpp"
#include <iostream>
#include <unistd.h>

int main(int argc, char *argv[]) {
    try {
        Editor e;
        if (argc >= 2) {
            e.openFile(argv[1]);
        }
        e.run();
    } catch (const std::system_error &e) {
        write(STDOUT_FILENO, "\x1b[2J", 4);
        write(STDOUT_FILENO, "\x1b[H", 3);
        std::cerr << "System/Terminal Error: " << e.what() << "\r\n";
        return 1;
    } catch (const std::exception &e) {
        write(STDOUT_FILENO, "\x1b[2J", 4);
        write(STDOUT_FILENO, "\x1b[H", 3);
        std::cerr << "Standard Library Exception: " << e.what() << "\r\n";
        return 1;
    } catch (...) {
        write(STDOUT_FILENO, "\x1b[2J", 4);
        write(STDOUT_FILENO, "\x1b[H", 3);
        std::cerr << "Unknown Fatal Error occurred.\r\n";
        return 1;
    }

    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
}
