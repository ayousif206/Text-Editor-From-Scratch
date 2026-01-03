#ifndef TEXT_EDITOR_FROM_SCRATCH_DATA_H
#define TEXT_EDITOR_FROM_SCRATCH_DATA_H

#include <vector>
#include <string>
#include <ctime>
#include <windows.h>

constexpr int ctrlKey(int k) {
    return k & 0x1f;
}

enum HighlightFlags {
    HL_HIGHLIGHT_NUMBERS = (1 << 0),
    HL_HIGHLIGHT_STRINGS = (1 << 1)
};

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
    editorSyntax *syntax = nullptr;

    HANDLE hIn = INVALID_HANDLE_VALUE;
    HANDLE hOut = INVALID_HANDLE_VALUE;
    DWORD orig_in_mode = 0;
    DWORD orig_out_mode = 0;
};

extern editorConfig E;
extern std::vector<editorSyntax> HLDB;

#endif