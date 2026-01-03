#include "Data.h"
#include "Terminal.h"
#include "FileIO.h"
#include "Output.h"
#include "Input.h"

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

    editorSetStatusMessage("Help: Ctrl-S = save | Ctrl-Q = quit | Ctrl-F = find");

    while (!E.quit) {
        editorRefreshScreen();
        editorProcessKeypress();
    }
    return 0;
}