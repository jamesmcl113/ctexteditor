#ifndef EDITOR_H
#define EDITOR_H

typedef struct {
    int length;
    char* data;
} erow;

typedef struct {
    int len;
    erow* rows;
} FileData;

typedef struct {
    int x;
    int y;
    int nrows;
    int ncols;
    int of;
    char fl[128];
    char commandBuf[3];
    FileData* fd;
} Editor;

void editorInit(Editor* e, int nrows, int ncols, char* fl);
void editorDie(const char* err);
void editorInsertChar(erow* row, int at, char c);

FileData* openFile(char* fn);

#endif