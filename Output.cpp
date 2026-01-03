#include "Output.h"
#include "Data.h"
#include "Row.h"
#include <cstdarg>
#include <cstdio>

void editorScroll() {
    E.rx = 0;
    if (E.cy < static_cast<int>(E.rows.size())) {
        E.rx = editorRowCxToRx(&E.rows[E.cy], E.cx);
    }

    if (E.cy < E.rowoff) {
        E.rowoff = E.cy;
    }
    if (E.cy >= E.rowoff + E.screenrows) {
        E.rowoff = E.cy - E.screenrows + 1;
    }
    if (E.rx < E.coloff) {
        E.coloff = E.rx;
    }
    if (E.rx >= E.coloff + E.screencols) {
        E.coloff = E.rx - E.screencols + 1;
    }
}

void editorDrawRows(abuf *ab) {
    for (int y = 0; y < E.screenrows; y++) {
        int filerow = y + E.rowoff;
        if (filerow >= static_cast<int>(E.rows.size())) {
            if (E.rows.empty() && y == E.screenrows / 3) {
                std::string welcome = "Welcome to my Text Editor!";
                if (static_cast<int>(welcome.size()) > E.screencols) welcome.resize(static_cast<size_t>(E.screencols));
                int padding = (E.screencols - static_cast<int>(welcome.size())) / 2;
                if (padding) { ab->append("~", 1); padding--; }
                while (padding--) ab->append(" ", 1);
                ab->append(welcome);
            } else {
                ab->append("~", 1);
            }
        } else {
            int len = static_cast<int>(E.rows[filerow].render.size()) - E.coloff;
            if (len < 0) len = 0;
            if (len > E.screencols) len = E.screencols;

            const char *c = E.rows[filerow].render.c_str() + E.coloff;
            const unsigned char *hl = E.rows[filerow].hl.data() + E.coloff;
            int current_color = -1;

            for (int j=0; j<len; j++) {
                if (iscntrl(static_cast<unsigned char>(c[j]))) {
                    char sym = (c[j] <= 26) ? static_cast<char>('@' + c[j]) : '?';
                    ab->append("\x1b[7m", 4);
                    ab->append(&sym, 1);
                    ab->append("\x1b[m", 3);
                    if (current_color != -1) {
                        char buf[16];
                        snprintf(buf, sizeof(buf), "\x1b[%dm", current_color);
                        ab->append(buf);
                    }
                } else if (hl[j] == HL_NORMAL) {
                    if (current_color != -1) {
                        ab->append("\x1b[39m");
                        current_color = -1;
                    }
                    ab->append(&c[j], 1);
                } else {
                    int color = editorSyntaxToColor(hl[j]);
                    if (color != current_color) {
                        current_color = color;
                        char buf[16];
                        snprintf(buf, sizeof(buf), "\x1b[%dm", color);
                        ab->append(buf);
                    }
                    ab->append(&c[j], 1);
                }
            }
            ab->append("\x1b[39m");
        }
        ab->append("\x1b[K");
        ab->append("\r\n");
    }
}

void editorDrawStatusBar(abuf *ab) {
    ab->append("\x1b[7m");
    std::string status = E.filename.empty() ? "[File Not Saved]" : E.filename;
    status += " - " + std::to_string(E.rows.size()) + " lines";
    status += E.dirty ? "(modified)" : "";

    std::string rstatus = (E.syntax ? E.syntax->filetype : "no type") + std::string(" | ") +
                          std::to_string(E.cy + 1) + "/" + std::to_string(E.rows.size());

    if (static_cast<int>(status.size()) > E.screencols) status.resize(static_cast<size_t>(E.screencols));
    ab->append(status);

    while (static_cast<int>(status.size()) < E.screencols) {
        if (E.screencols - static_cast<int>(status.size()) == static_cast<int>(rstatus.size())) {
            ab->append(rstatus);
            break;
        }
        ab->append(" ", 1);
        status += " ";
    }
    ab->append("\x1b[m\r\n");
}

void editorDrawMessageBar(abuf *ab) {
    ab->append("\x1b[K");
    if (time(nullptr) - E.statusmsg_time < 5) ab->append(E.statusmsg);
}

void editorRefreshScreen() {
    editorScroll();

    abuf ab;
    ab.append("\x1b[?25l");
    ab.append("\x1b[H");

    editorDrawRows(&ab);
    editorDrawStatusBar(&ab);
    editorDrawMessageBar(&ab);

    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy - E.rowoff) + 1, (E.rx - E.coloff) + 1);
    ab.append(buf);

    ab.append("\x1b[?25h");

    DWORD written;
    WriteConsoleA(E.hOut, ab.b.c_str(), ab.b.size(), &written, nullptr);
}

void editorSetStatusMessage(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    char buf[80];
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    E.statusmsg = buf;
    E.statusmsg_time = time(nullptr);
}