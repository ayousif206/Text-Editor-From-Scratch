#ifndef TEXT_EDITOR_FROM_SCRATCH_TERMINAL_H
#define TEXT_EDITOR_FROM_SCRATCH_TERMINAL_H

[[noreturn]] void die(const char *s);
void disableRawMode();
void enableRawMode();
int editorReadKey();
int getWindowSize(int *rows, int *cols);

#endif