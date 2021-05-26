#include <ncurses.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#include "editor.h"

#define CTRL(c) ((c) & 0x1f)

Editor e;

/*
TODO:
    Add error function
    Rename File struct 

*/

void die(const char *s)
{
    perror(s);
    endwin();
    exit(1);
}

typedef struct {
    char* data;
    int len;
} buf;

#define BUF_INIT {NULL, 0}

void appendBuf(buf* b, char* str)
{
    char* new = realloc(b->data, b->len + strlen(str));
    if(new == NULL) die("realloc buf");

    memcpy(&new[b->len], str, strlen(str));
    b->data = new;
    b->len += strlen(str);
}

void freeBuf(buf* b)
{
    free(b->data);
}

void drawRows(buf* b)
{
    move(0, 0);
    for(int i = 0; i < e.nrows - 1; i++)
    {
        clrtoeol();

        // draw welcome message
        if(i == e.nrows / 3 && !strcmp(e.fl, "NO FILE"))
        {
            char msg[] = "*------ XEDITOR V0.0.1 ------*\n";
            int padding = (e.ncols - strlen(msg)) / 2;
            if(padding)
            {
                appendBuf(b, "~");
                padding--;
            }
            while(padding--) appendBuf(b, " ");
            appendBuf(b, msg);
        } else if(i < e.fd->len) {
            if(strlen(e.fd->rows[i + e.of].data) > e.ncols) e.fd->rows[i + e.of].data[e.ncols - 1] = 0;
            appendBuf(b, e.fd->rows[i + e.of].data);
            appendBuf(b, "\n");
        }  else {
            appendBuf(b, "~\n");
        }
    }
}

void moveCursor(int c);

void executeCommand(int c)
{
    char x = c;

    strncat(e.commandBuf, &x, 1);

    int executed = 0;
    if(!strcmp(e.commandBuf, "G")) moveCursor('.');
    else if(!strcmp(e.commandBuf, "gg")) moveCursor('/');
    else executed = 1;

    if(strlen(e.commandBuf) == 2 || !executed) e.commandBuf[0] = '\0';
}

void handleKeypress(int c)
{
    if(c == KEY_DOWN || c == KEY_UP || c == KEY_RIGHT || c == KEY_LEFT) moveCursor(c);
    //else executeCommand(c);
    else if(c == 127) editorDelChar(&e.fd->rows[e.y + e.of], e.x);
    else if(c > 31 && e.fd->len > 0) editorInsertChar(&e.fd->rows[e.y + e.of], e.x, c);
    mvprintw(0, 0, "%i", c);
}

void moveCursor(int c)
{
    int ncols, nrows, y, x;
    getmaxyx(stdscr, nrows, ncols);
    getyx(stdscr, y, x);

    int maxOffset = (e.fd->len - (nrows - 1));
    if(e.fd->len < nrows - 1) maxOffset = 0;

    switch(c)
    {
        case KEY_UP:
            if(e.y <= 0) {
                e.y = 0;
                if(e.of > 0) e.of--;
            } else e.y--;
            break;
        case KEY_DOWN:
            if(e.y + e.of == e.fd->len - 1) ;
            else if(e.y == nrows - 2 && e.y + e.of < e.fd->len) e.of++;
            else if(e.y == nrows - 2 && e.fd->len < nrows - 2) ;
            else e.y++;
            break;
        case KEY_LEFT:
            if(e.fd->len > 0) {
                if(e.x <= 0)
                { 
                    e.x = 0;
                    if(e.y > 0) 
                    { 
                        e.y--;
                        e.x = e.fd->rows[e.y + e.of].length - 1;
                    }
                } else e.x--;
            }
            break;
        case KEY_RIGHT:
            if(e.fd->len > 0) {
                if(e.x == e.fd->rows[e.y + e.of].length - 1 && e.y + e.of < e.fd->len - 1)
                {
                    e.y++;
                    e.x = 0;
                } else if(e.x == e.fd->rows[e.y + e.of].length - 1) ;
                else e.x++;
            }
            break;
        case '.':
            e.of = maxOffset;
            y = nrows - 2;
            break;
        case '/':
            e.of = 0;
            y = 0;
            break;
        default:
            return;
    }

    if(e.fd->len > 0) {
        if(e.x > e.fd->rows[e.y + e.of].length - 1) e.x = e.fd->rows[e.y + e.of].length - 1;
    }

    move(e.y, e.x);
}

void setStatusMessage(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(e.statusmsg, sizeof(e.statusmsg), fmt, ap);
    va_end(ap);
}

void drawStatusLine()
{
    attrset(A_BOLD);
    move(e.nrows - 1, 0);
    clrtoeol();
    mvprintw(e.nrows - 1, 0, "> %s", e.fl);
    mvprintw(e.nrows - 1, e.ncols / 4, "%d , %d / %d", e.y + e.of + 1, e.x + 1, e.fd->len);
    mvprintw(e.nrows - 1, e.ncols / 2, "|");
    mvprintw(e.nrows - 1, (e.ncols / 2) + 4, "%s", e.statusmsg);
    attrset(A_NORMAL);
}

void refreshScreen(FileData* fd)
{
    buf b = BUF_INIT;

    // get row data and output to screen
    drawRows(&b);
    mvprintw(0, 0, b.data);

    drawStatusLine();
    move(e.y, e.x);
}

int main(int argc, char* argv[])
{
    initscr();
    noecho();
    raw();
    keypad(stdscr, TRUE);

    int nrows = getmaxy(stdscr);
    int ncols = getmaxx(stdscr);

    if(argc == 2) editorInit(&e, nrows, ncols, argv[1]);
    else editorInit(&e, nrows, ncols, "NONE");

    setStatusMessage("HELP: CTRL-Q to quit");

    int c;
    refreshScreen(e.fd);
    while((c = getch()) != CTRL('q'))
    {
        handleKeypress(c);
        refreshScreen(e.fd);
    }
    
    endwin();
    return 0;
}