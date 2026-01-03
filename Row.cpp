#include "Row.h"
#include <algorithm>
#include <cctype>

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
                }
                i++;
                continue;
            }
            if (row->render.substr(i, mcs.size()) == mcs) {
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
            }
            if (c == '"' || c == '\'') {
                in_string = c;
                row->hl[i] = HL_STRING;
                i++;
                continue;
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
        case HL_MLCOMMENT: return 36;
        case HL_KEYWORD1: return 33;
        case HL_KEYWORD2: return 32;
        case HL_STRING: return 35;
        case HL_NUMBER: return 31;
        case HL_MATCH: return 34;
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