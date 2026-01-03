#include "Input.h"
#include "Data.h"
#include "Terminal.h"
#include "Row.h"
#include "Output.h"
#include "FileIO.h"

std::string editorPrompt(const char *prompt) {
    std::string buf;
    while (true) {
        editorSetStatusMessage(prompt, buf.c_str());
        editorRefreshScreen();

        int c = editorReadKey();
        if (c == DEL_KEY || c == BACKSPACE) {
            if (!buf.empty()) buf.pop_back();
        } else if (c == '\x1b') {
            editorSetStatusMessage("");
            return "";
        } else if (c == '\r') {
            if (!buf.empty()) {
                editorSetStatusMessage("");
                return buf;
            }
        } else if (!iscntrl(c)) {
            buf += static_cast<char>(c);
        }
    }
}

void editorFind() {
    int saved_cx = E.cx, saved_cy = E.cy;
    int saved_coloff = E.coloff, saved_rowoff = E.rowoff;

    std::string query = editorPrompt("Search for: %s (ESC to cancel)");

    if (query.empty()) {
        E.cx = saved_cx; E.cy = saved_cy;
        E.coloff = saved_coloff; E.rowoff = saved_rowoff;
        return;
    }

    for (int i = 0; i < static_cast<int>(E.rows.size()); i++) {
        int rowIdx = (i + E.cy) % static_cast<int>(E.rows.size());
        erow *row = &E.rows[rowIdx];
        size_t match = row->render.find(query);
        if (match != std::string::npos) {
            E.cy = rowIdx;
            E.cx = editorRowRxToCx(row, static_cast<int>(match));
            E.rowoff = static_cast<int>(E.rows.size());

            std::fill_n(row->hl.begin() + static_cast<ptrdiff_t>(match), query.size(), HL_MATCH);
            break;
        }
    }
}

void editorMoveCursor(int key) {
    erow *row = (E.cy >= static_cast<int>(E.rows.size())) ? nullptr : &E.rows[E.cy];

    switch (key) {
        case ARROW_LEFT:
            if (E.cx != 0) E.cx--;
            else if (E.cy > 0) {
                E.cy--;
                E.cx = static_cast<int>(E.rows[E.cy].chars.size());
            }
            break;
        case ARROW_RIGHT:
            if (row && E.cx < static_cast<int>(row->chars.size())) E.cx++;
            else if (row && E.cx == static_cast<int>(row->chars.size())) { E.cy++; E.cx = 0; }
            break;
        case ARROW_UP:
            if (E.cy != 0) E.cy--;
            break;
        case ARROW_DOWN:
            if (E.cy < static_cast<int>(E.rows.size())) E.cy++;
            break;
        default: break;
    }

    row = (E.cy >= static_cast<int>(E.rows.size())) ? nullptr : &E.rows[E.cy];
    int rowlen = row ? static_cast<int>(row->chars.size()) : 0;
    if (E.cx > rowlen) E.cx = rowlen;
}

void editorProcessKeypress() {
    static int quit_times = 3;

    switch (int c = editorReadKey()) {
        case '\r':
            editorInsertNewline();
            break;

        case ctrlKey('q'):
            if (E.dirty && quit_times > 0) {
                editorSetStatusMessage("WARNING - UNSAVED CHANGES. "
                    "Hit Ctrl-Q %d times to quit.", quit_times);
                quit_times--;
                return;
            }
            {
                DWORD written;
                WriteConsoleA(E.hOut, "\x1b[2J", 4, &written, nullptr);
                WriteConsoleA(E.hOut, "\x1b[H", 3, &written, nullptr);
            }
            E.quit = true;
            break;

        case ctrlKey('s'):
            if (E.filename.empty()) {
                E.filename = editorPrompt("Save as: %s");
                if (E.filename.empty()) {
                    editorSetStatusMessage("Save cancelled");
                    return;
                }
                editorSelectSyntaxHighlight();
            }
            editorSave();
            break;

        case ctrlKey('f'):
            editorFind();
            break;

        case HOME_KEY:
            E.cx = 0;
            break;
        case END_KEY:
            if (E.cy < static_cast<int>(E.rows.size())) E.cx = static_cast<int>(E.rows[E.cy].chars.size());
            break;

        case BACKSPACE:
        case ctrlKey('h'):
        case DEL_KEY:
            if (c == DEL_KEY) editorMoveCursor(ARROW_RIGHT);
            editorDelChar();
            break;

        case PAGE_UP:
        case PAGE_DOWN:
            {
                if (c == PAGE_UP) E.cy = E.rowoff;
                else {
                    E.cy = E.rowoff + E.screenrows - 1;
                    if (E.cy > static_cast<int>(E.rows.size())) E.cy = static_cast<int>(E.rows.size());
                }
                int times = E.screenrows;
                while (times--) editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
            }
            break;

        case ARROW_UP:
        case ARROW_DOWN:
        case ARROW_LEFT:
        case ARROW_RIGHT:
            editorMoveCursor(c);
            break;

        case ctrlKey('l'):
        case '\x1b':
            break;

        default:
            editorInsertChar(c);
            break;
    }
    quit_times = 3;
}