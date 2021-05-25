#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ncurses.h>

#include "editor.h"

void editorInit(Editor* e, int nrows, int ncols, char* fl)
{
    e->x = 0;
    e->y = 0;
    e->nrows = nrows;
    e->ncols = ncols;
    e->of = 0;
    strcpy(e->fl, fl);
    e->fd = openFile(fl);
}

void editorDie(const char* err)
{
    perror(err);
    endwin();
    exit(0);
}

void editorInsertChar(erow* row, int at, char c)
{
    if(at < 0 || at > row->length) at = row->length;
    row->data = realloc(row->data, row->length + 2);
    memmove(&row->data[at + 1], &row->data[at], row->length - at + 1);
    row->length++;
    row->data[at] = c;
    row->data[row->length] = '\0';
}   

FileData* openFile(char* fn)
{
    if(!strcmp(fn, "NONE")) return NULL;

    FILE* fp = fopen(fn, "rb");
    if(fp == NULL) editorDie("fopen");

    FileData* fd = malloc(sizeof *fd);
    if(fd == NULL) editorDie("malloc filedata");
    fd->len = 0;
    fd->rows = NULL;

    char* line = NULL;
    size_t linecap = 0;
    ssize_t linelen;
    while((linelen = getline(&line, &linecap, fp)) != -1) 
    {
        while(linelen > 0 && (line[linelen - 1] == '\n' || line[linelen - 1] == '\r')) linelen--;

        erow* tmp = realloc(fd->rows, (fd->len + 1) * sizeof(erow));
        if(tmp == NULL) editorDie("realloc rows");
        fd->rows = tmp;

        fd->rows[fd->len].data = malloc(linelen + 1);
        memcpy(fd->rows[fd->len].data, line, linelen);
        fd->rows[fd->len].data[linelen] = '\0';
        fd->rows[fd->len].length = linelen;
        fd->len++;
    }

    free(line);
    fclose(fp);

    return fd;
}