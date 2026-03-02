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
        std::cerr << "System/Terminal Error: " << e.what() << "\r\n";
    } catch (const std::exception &e) {
        std::cerr << "Standard Library Exception: " << e.what() << "\r\n";
    } catch (...) {
        std::cerr << "Unknown Fatal Error occurred.\r\n";
    }

    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
}
