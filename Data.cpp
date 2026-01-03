#include "Data.h"

editorConfig E;

std::vector<std::string> C_HL_extensions = {".c", ".h", ".cpp", ".hpp", ".cc"};
std::vector<std::string> C_HL_keywords = {
    "switch", "if", "while", "for", "break", "continue", "return", "else",
    "struct", "union", "typedef", "static", "enum", "class", "case",
    "int|", "long|", "double|", "float|", "char|", "unsigned|", "signed|",
    "void|", "bool|", "auto|", "const|", "template|", "typename|"
};

std::vector<editorSyntax> HLDB = {
    {
        "c",
        C_HL_extensions,
        C_HL_keywords,
        "//", "/*", "*/",
        HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS
    }
};