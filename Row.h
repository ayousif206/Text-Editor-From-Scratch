#ifndef TEXT_EDITOR_FROM_SCRATCH_ROW_H
#define TEXT_EDITOR_FROM_SCRATCH_ROW_H

#include "Data.h"

int editorRowCxToRx(const erow *row, int cx);
int editorRowRxToCx(const erow *row, int rx);
void editorUpdateRow(erow *row);
void editorInsertRow(int at, const std::string& s);
void editorFreeRow(int at);
void editorRowInsertChar(erow *row, int at, int c);
void editorRowAppendString(erow *row, const std::string& s);
void editorRowDelChar(erow *row, int at);
void editorInsertChar(int c);
void editorInsertNewline();
void editorDelChar();
void editorUpdateSyntax(erow *row);
void editorSelectSyntaxHighlight();
int editorSyntaxToColor(int hl);

#endif