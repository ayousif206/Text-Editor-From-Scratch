#include "FileIO.h"
#include "Data.h"
#include "Row.h"
#include "Terminal.h"
#include "Output.h"
#include <fstream>

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
            editorSetStatusMessage("%d bytes written", content.size());
            return;
        }
    }
    editorSetStatusMessage("I/O error");
}