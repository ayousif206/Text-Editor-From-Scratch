#ifndef TEXT_EDITOR_FROM_SCRATCH_INPUT_H
#define TEXT_EDITOR_FROM_SCRATCH_INPUT_H

#include <string>

std::string editorPrompt(const char *prompt);
void editorFind();
void editorMoveCursor(int key);
void editorProcessKeypress();

#endif