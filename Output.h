#ifndef TEXT_EDITOR_FROM_SCRATCH_OUTPUT_H
#define TEXT_EDITOR_FROM_SCRATCH_OUTPUT_H

#include <string>

struct abuf {
    std::string b;

    void append(const char *s, int len) {
        b.append(s, static_cast<size_t>(len));
    }

    void append(const std::string& s) {
        b += s;
    }
};

void editorScroll();
void editorDrawRows(abuf *ab);
void editorDrawStatusBar(abuf *ab);
void editorDrawMessageBar(abuf *ab);
void editorRefreshScreen();
void editorSetStatusMessage(const char *fmt, ...);

#endif