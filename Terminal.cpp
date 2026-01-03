#include "Terminal.h"
#include "Data.h"
#include <cstdio>
#include <iostream>

[[noreturn]] void die(const char *s) {
    DWORD written;
    WriteConsoleA(E.hOut, "\x1b[2J", 4, &written, nullptr);
    WriteConsoleA(E.hOut, "\x1b[H", 3, &written, nullptr);
    perror(s);
    exit(1);
}

void disableRawMode() {
    if (E.hIn != INVALID_HANDLE_VALUE && E.hOut != INVALID_HANDLE_VALUE) {
        SetConsoleMode(E.hIn, E.orig_in_mode);
        SetConsoleMode(E.hOut, E.orig_out_mode);
    }
}

void enableRawMode() {
    E.hIn = GetStdHandle(STD_INPUT_HANDLE);
    E.hOut = GetStdHandle(STD_OUTPUT_HANDLE);

    if (E.hIn == INVALID_HANDLE_VALUE || E.hOut == INVALID_HANDLE_VALUE) die("GetStdHandle");

    if (!GetConsoleMode(E.hIn, &E.orig_in_mode)) die("GetConsoleMode");
    if (!GetConsoleMode(E.hOut, &E.orig_out_mode)) die("GetConsoleMode");

    DWORD out_mode = E.orig_out_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING | DISABLE_NEWLINE_AUTO_RETURN;
    if (!SetConsoleMode(E.hOut, out_mode)) die("SetConsoleMode (VT)");

    DWORD in_mode = E.orig_in_mode;
    in_mode &= ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT);
    in_mode |= ENABLE_VIRTUAL_TERMINAL_INPUT;
    if (!SetConsoleMode(E.hIn, in_mode)) die("SetConsoleMode");

    atexit(disableRawMode);
}

int editorReadKey() {
    char c = 0;
    DWORD read;
    while (ReadFile(E.hIn, &c, 1, &read, nullptr) == 0 || read != 1) {
        //TBA
    }

    if (c == '\x1b') {
        char seq[3];
        if (ReadFile(E.hIn, &seq[0], 1, &read, nullptr) == 0 || read != 1) return '\x1b';
        if (ReadFile(E.hIn, &seq[1], 1, &read, nullptr) == 0 || read != 1) return '\x1b';

        if (seq[0] == '[') {
            if (seq[1] >= '0' && seq[1] <= '9') {
                if (ReadFile(E.hIn, &seq[2], 1, &read, nullptr) == 0 || read != 1) return '\x1b';
                if (seq[2] == '~') {
                    switch (seq[1]) {
                        case '1': return HOME_KEY;
                        case '3': return DEL_KEY;
                        case '4': return END_KEY;
                        case '5': return PAGE_UP;
                        case '6': return PAGE_DOWN;
                        case '7': return HOME_KEY;
                        case '8': return END_KEY;
                        default: return '\x1b';
                    }
                }
            } else {
                switch (seq[1]) {
                    case 'A': return ARROW_UP;
                    case 'B': return ARROW_DOWN;
                    case 'C': return ARROW_RIGHT;
                    case 'D': return ARROW_LEFT;
                    case 'H': return HOME_KEY;
                    case 'F': return END_KEY;
                    default: return '\x1b';
                }
            }
        } else if (seq[0] == 'O') {
            switch (seq[1]) {
                case 'H': return HOME_KEY;
                case 'F': return END_KEY;
                default: return '\x1b';
            }
        }
        return '\x1b';
    }
    return c;
}

int getWindowSize(int *rows, int *cols) {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(E.hOut, &csbi)) {
        *cols = csbi.srWindow.Right - csbi.srWindow.Left + 1;
        *rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
        return 0;
    }
    return -1;
}