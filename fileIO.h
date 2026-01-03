#ifndef TEXT_EDITOR_FROM_SCRATCH_FILEIO_H
#define TEXT_EDITOR_FROM_SCRATCH_FILEIO_H

#include <string>

void editorOpen(const char *filename);
std::string editorRowsToString();
void editorSave();

#endif