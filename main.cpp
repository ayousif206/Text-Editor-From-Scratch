#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <ctime>

#include <windows.h>
#include <io.h>

#define CTRL_KEY(k) ((k) & 0x1f)

enum editorKey {
    BACKSPACE = 127,
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    DEL_KEY,
    HOME_KEY,
    END_KEY,
    PAGE_UP,
    PAGE_DOWN
};

enum editorHighlight {
    HL_NORMAL = 0,
    HL_COMMENT,
    HL_MLCOMMENT,
    HL_KEYWORD1,
    HL_KEYWORD2,
    HL_STRING,
    HL_NUMBER,
    HL_MATCH
};

#define HL_HIGHLIGHT_NUMBERS (1<<0)
#define HL_HIGHLIGHT_STRINGS (1<<1)

//data init
struct editorSyntax {
    std::string filetype;
    std::vector<std::string> filematch;
    std::vector<std::string> keywords;
    std::string singleline_comment_start;
    std::string multiline_comment_start;
    std::string multiline_comment_end;
    int flags;
};

struct erow {
    int idx = 0;
    std::string chars;
    std::string render;
    std::vector<unsigned char> hl;
    bool hl_open_comment = false;
};

struct editorConfig {
    int cx = 0, cy = 0;
    int rx = 0;
    int rowoff = 0;
    int coloff = 0;
    int screenrows = 0;
    int screencols = 0;
    int dirty = 0;
    bool quit = false;
    std::vector<erow> rows;
    std::string filename;
    std::string statusmsg;
    time_t statusmsg_time = 0;
    struct editorSyntax *syntax = nullptr;

    //windows console
    HANDLE hIn = INVALID_HANDLE_VALUE;
    HANDLE hOut = INVALID_HANDLE_VALUE;
    DWORD orig_in_mode = 0;
    DWORD orig_out_mode = 0;
};

struct editorConfig E;

//file types
std::vector<std::string> C_HL_extensions = {".c", ".h", ".cpp", ".hpp", ".cc"};
std::vector<std::string> C_HL_keywords = {
    "switch", "if", "while", "for", "break", "continue", "return", "else",
    "struct", "union", "typedef", "static", "enum", "class", "case",
    "int|", "long|", "double|", "float|", "char|", "unsigned|", "signed|",
    "void|", "bool|", "auto|", "const|", "template|", "typename|"
};

struct editorSyntax HLDB[] = {
    {
        "c",
        C_HL_extensions,
        C_HL_keywords,
        "//", "/*", "*/",
        HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS
    },
};

#define HLDB_ENTRIES (sizeof(HLDB) / sizeof(HLDB[0]))

//terminal
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
        //tbc
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
        *cols = static_cast<int>(csbi.srWindow.Right - csbi.srWindow.Left + 1);
        *rows = static_cast<int>(csbi.srWindow.Bottom - csbi.srWindow.Top + 1);
        return 0;
    }
    return -1;
}

//syntax highlight
int is_separator(int c) {
    return isspace(static_cast<unsigned char>(c)) || c == '\0' || strchr(",.()+-/*=~%<>[];", c) != nullptr;
}

void editorUpdateSyntax(erow *row) {
    row->hl.assign(row->render.size(), HL_NORMAL);
    if (E.syntax == nullptr) return;

    std::string scs = E.syntax->singleline_comment_start;
    std::string mcs = E.syntax->multiline_comment_start;
    std::string mce = E.syntax->multiline_comment_end;

    int prev_sep = 1;
    char in_string = 0;
    bool in_comment = (row->idx > 0 && E.rows[row->idx - 1].hl_open_comment);

    size_t i = 0;
    while (i < row->render.size()) {
        char c = row->render[i];
        unsigned char prev_hl = (i > 0) ? row->hl[i - 1] : HL_NORMAL;

        if (!scs.empty() && !in_string && !in_comment) {
            if (row->render.substr(i, scs.size()) == scs) {
                std::fill(row->hl.begin() + static_cast<ptrdiff_t>(i), row->hl.end(), HL_COMMENT);
                break;
            }
        }

        if (!mcs.empty() && !mce.empty() && !in_string) {
            if (in_comment) {
                row->hl[i] = HL_MLCOMMENT;
                if (row->render.substr(i, mce.size()) == mce) {
                    std::fill_n(row->hl.begin() + static_cast<ptrdiff_t>(i), mce.size(), HL_MLCOMMENT);
                    i += mce.size();
                    in_comment = false;
                    prev_sep = 1;
                    continue;
                } else {
                    i++;
                    continue;
                }
            } else if (row->render.substr(i, mcs.size()) == mcs) {
                std::fill_n(row->hl.begin() + static_cast<ptrdiff_t>(i), mcs.size(), HL_MLCOMMENT);
                i += mcs.size();
                in_comment = true;
                continue;
            }
        }

        if (E.syntax->flags & HL_HIGHLIGHT_STRINGS) {
            if (in_string) {
                row->hl[i] = HL_STRING;
                if (c == '\\' && i + 1 < row->render.size()) {
                    row->hl[i + 1] = HL_STRING;
                    i += 2;
                    continue;
                }
                if (c == in_string) in_string = 0;
                i++;
                prev_sep = 1;
                continue;
            } else {
                if (c == '"' || c == '\'') {
                    in_string = c;
                    row->hl[i] = HL_STRING;
                    i++;
                    continue;
                }
            }
        }

        if (E.syntax->flags & HL_HIGHLIGHT_NUMBERS) {
            if ((isdigit(static_cast<unsigned char>(c)) && (prev_sep || prev_hl == HL_NUMBER)) ||
                (c == '.' && prev_hl == HL_NUMBER)) {
                row->hl[i] = HL_NUMBER;
                i++;
                prev_sep = 0;
                continue;
            }
        }

        if (prev_sep) {
            bool keyword_found = false;
            for (const auto &kw_str : E.syntax->keywords) {
                std::string kw = kw_str;
                bool is_type = (kw.back() == '|');
                if (is_type) kw.pop_back();

                if (row->render.substr(i, kw.size()) == kw &&
                    (i + kw.size() >= row->render.size() || is_separator(row->render[i + kw.size()]))) {
                    std::fill_n(row->hl.begin() + static_cast<ptrdiff_t>(i), kw.size(), is_type ? HL_KEYWORD2 : HL_KEYWORD1);
                    i += kw.size();
                    prev_sep = 0;
                    keyword_found = true;
                    break;
                }
            }
            if (keyword_found) continue;
        }

        prev_sep = is_separator(c);
        i++;
    }

    bool changed = (row->hl_open_comment != in_comment);
    row->hl_open_comment = in_comment;
    if (changed && static_cast<size_t>(row->idx) + 1 < E.rows.size())
        editorUpdateSyntax(&E.rows[row->idx + 1]);
}

int editorSyntaxToColor(int hl) {
    switch (hl) {
        case HL_COMMENT:
        case HL_MLCOMMENT: return 36; //cyan
        case HL_KEYWORD1: return 33;  //yellow
        case HL_KEYWORD2: return 32;  //green
        case HL_STRING: return 35;    //magenta
        case HL_NUMBER: return 31;    //red
        case HL_MATCH: return 34;     //blue
        default: return 37;
    }
}

void editorSelectSyntaxHighlight() {
    E.syntax = nullptr;
    if (E.filename.empty()) return;

    size_t dot = E.filename.find_last_of('.');
    std::string ext = (dot != std::string::npos) ? E.filename.substr(dot) : "";

    for (auto &s : HLDB) {
        for (const auto& m : s.filematch) {
            if (m == ext) {
                E.syntax = &s;
                for (auto& row : E.rows) editorUpdateSyntax(&row);
                return;
            }
        }
    }
}

//row manipulators
int editorRowCxToRx(const erow *row, int cx) {
    int rx = 0;
    for (int j = 0; j < cx; j++) {
        if (row->chars[j] == '\t')
            rx += (4 - 1) - (rx % 4);
        rx++;
    }
    return rx;
}

int editorRowRxToCx(const erow *row, int rx) {
    int cur_rx = 0;
    int cx;
    for (cx = 0; cx < static_cast<int>(row->chars.size()); cx++) {
        if (row->chars[cx] == '\t')
            cur_rx += (4 - 1) - (cur_rx % 4);
        cur_rx++;
        if (cur_rx > rx) return cx;
    }
    return cx;
}

void editorUpdateRow(erow *row) {
    std::string render;
    for (char c : row->chars) {
        if (c == '\t') {
            render += ' ';
            while (render.size() % 4 != 0) render += ' ';
        } else {
            render += c;
        }
    }
    row->render = render;
    editorUpdateSyntax(row);
}

void editorInsertRow(int at, const std::string& s) {
    if (at < 0 || at > static_cast<int>(E.rows.size())) return;
    erow row;
    row.idx = at;
    row.chars = s;
    row.hl_open_comment = false;
    editorUpdateRow(&row);
    E.rows.insert(E.rows.begin() + at, row);
    for (int j = at + 1; j < static_cast<int>(E.rows.size()); j++) E.rows[j].idx++;
    E.dirty++;
}

void editorFreeRow(int at) {
    if (at < 0 || at >= static_cast<int>(E.rows.size())) return;
    E.rows.erase(E.rows.begin() + at);
    for (int j = at; j < static_cast<int>(E.rows.size()); j++) E.rows[j].idx--;
    E.dirty++;
}

void editorRowInsertChar(erow *row, int at, int c) {
    if (at < 0 || at > static_cast<int>(row->chars.size())) at = static_cast<int>(row->chars.size());
    row->chars.insert(static_cast<size_t>(at), 1, static_cast<char>(c));
    editorUpdateRow(row);
    E.dirty++;
}

void editorRowAppendString(erow *row, const std::string& s) {
    row->chars += s;
    editorUpdateRow(row);
    E.dirty++;
}

void editorRowDelChar(erow *row, int at) {
    if (at < 0 || at >= static_cast<int>(row->chars.size())) return;
    row->chars.erase(static_cast<size_t>(at), 1);
    editorUpdateRow(row);
    E.dirty++;
}

//editor manipulators
void editorInsertChar(int c) {
    if (E.cy == static_cast<int>(E.rows.size())) editorInsertRow(static_cast<int>(E.rows.size()), "");
    editorRowInsertChar(&E.rows[E.cy], E.cx, c);
    E.cx++;
}

void editorInsertNewline() {
    if (E.cx == 0) {
        editorInsertRow(E.cy, "");
    } else {
        erow *row = &E.rows[E.cy];
        editorInsertRow(E.cy + 1, row->chars.substr(static_cast<size_t>(E.cx)));
        row = &E.rows[E.cy];
        row->chars = row->chars.substr(0, static_cast<size_t>(E.cx));
        editorUpdateRow(row);
    }
    E.cy++;
    E.cx = 0;
}

void editorDelChar() {
    if (E.cy == static_cast<int>(E.rows.size())) return;
    if (E.cx == 0 && E.cy == 0) return;

    if (E.cx > 0) {
        editorRowDelChar(&E.rows[E.cy], E.cx - 1);
        E.cx--;
    } else {
        E.cx = static_cast<int>(E.rows[E.cy - 1].chars.size());
        editorRowAppendString(&E.rows[E.cy - 1], E.rows[E.cy].chars);
        editorFreeRow(E.cy);
        E.cy--;
    }
}

//file io
void editorSetStatusMessage(const char *fmt, ...);

void editorOpen(const char *filename) {
    E.filename = filename;
    editorSelectSyntaxHighlight();

    std::ifstream file(filename);
    if (!file.is_open()) die("fopen");

    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        editorInsertRow(static_cast<int>(E.rows.size()), line);
    }
    E.dirty = 0;
}

std::string editorRowsToString() {
    std::string out;
    for (const auto& row : E.rows) {
        out += row.chars + "\n";
    }
    return out;
}

void editorSave() {
    if (E.filename.empty()) return;

    std::ofstream file(E.filename);
    if (file.is_open()) {
        std::string content = editorRowsToString();
        file << content;

        if (file.good()) {
            E.dirty = 0;
            editorSetStatusMessage("%d bytes written to disk", content.size());
            return;
        }
    }
    editorSetStatusMessage("Can't save! I/O error");
}

//buffer

struct abuf {
    std::string b;

    void append(const char *s, int len) {
        b.append(s, static_cast<size_t>(len));
    }

    void append(const std::string& s) {
        b += s;
    }
};

//output

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

void editorDrawRows(struct abuf *ab) {
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

void editorDrawStatusBar(struct abuf *ab) {
    ab->append("\x1b[7m");
    std::string status = E.filename.empty() ? "[No Name]" : E.filename;
    status += " - " + std::to_string(E.rows.size()) + " lines";
    status += E.dirty ? "(modified)" : "";

    std::string rstatus = (E.syntax ? E.syntax->filetype : "no ft") + std::string(" | ") +
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

void editorDrawMessageBar(struct abuf *ab) {
    ab->append("\x1b[K");
    if (time(nullptr) - E.statusmsg_time < 5) ab->append(E.statusmsg);
}

void editorRefreshScreen() {
    editorScroll();

    struct abuf ab;
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
    WriteConsoleA(E.hOut, ab.b.c_str(), static_cast<DWORD>(ab.b.size()), &written, nullptr);
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

//input

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

    std::string query = editorPrompt("Search: %s (ESC to cancel)");

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

        case CTRL_KEY('q'):
            if (E.dirty && quit_times > 0) {
                editorSetStatusMessage("WARNING!!! File has unsaved changes. "
                    "Press Ctrl-Q %d more times to quit.", quit_times);
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

        case CTRL_KEY('s'):
            if (E.filename.empty()) {
                E.filename = editorPrompt("Save as: %s");
                if (E.filename.empty()) {
                    editorSetStatusMessage("Save aborted");
                    return;
                }
                editorSelectSyntaxHighlight();
            }
            editorSave();
            break;

        case CTRL_KEY('f'):
            editorFind();
            break;

        case HOME_KEY:
            E.cx = 0;
            break;
        case END_KEY:
            if (E.cy < static_cast<int>(E.rows.size())) E.cx = static_cast<int>(E.rows[E.cy].chars.size());
            break;

        case BACKSPACE:
        case CTRL_KEY('h'):
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

        case CTRL_KEY('l'):
        case '\x1b':
            break;

        default:
            editorInsertChar(c);
            break;
    }
    quit_times = 3;
}

//init
void initEditor() {
    E.cx = 0;
    E.cy = 0;
    E.rx = 0;
    E.rowoff = 0;
    E.coloff = 0;
    E.dirty = 0;
    E.quit = false;
    E.statusmsg_time = 0;
    E.syntax = nullptr;

    if (getWindowSize(&E.screenrows, &E.screencols) == -1) die("getWindowSize");
    E.screenrows -= 2;
}

int main(int argc, char *argv[]) {
    enableRawMode();
    initEditor();
    if (argc >= 2) {
        editorOpen(argv[1]);
    }

    editorSetStatusMessage("HELP: Ctrl-S = save | Ctrl-Q = quit | Ctrl-F = find");

    while (!E.quit) {
        editorRefreshScreen();
        editorProcessKeypress();
    }
    return 0;
}