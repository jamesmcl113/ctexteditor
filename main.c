#include <ncurses.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#define CTRL(c) ((c) & 0x1f)

/*
TODO:
    Add file saving, output etc..
*/

/** structs **/
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
    char statusmsg[64];
    FileData* fd;
} Editor;

Editor e;
int editFlag = 0;

void die(const char *s)
{
    perror(s);
    endwin();
    exit(1);
}

/** buffer **/
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

/** output **/
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

void drawStatusLine()
{
    attrset(A_BOLD);
    move(e.nrows - 1, 0);
    clrtoeol();
    mvprintw(e.nrows - 1, 0, "> [%s], %d lines", e.fl, e.fd->len);
    mvprintw(e.nrows - 1, e.ncols / 2, "%d , %d", e.y + e.of + 1, e.x + 1);
    //mvprintw(e.nrows - 1, e.ncols / 2, "|");
    mvprintw(e.nrows - 1, e.ncols - 30, "%s", e.statusmsg);
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

/** file i/o **/
FileData* openFile(char* fn)
{
    if(!strcmp(fn, "NONE"))
    {
        FileData* fd = malloc(sizeof *fd);
        fd->len = 0;
        fd->rows = NULL;
        return fd;
    }

    FILE* fp = fopen(fn, "rb");
    if(fp == NULL) die("fopen");

    FileData* fd = malloc(sizeof *fd);
    if(fd == NULL) die("malloc filedata");
    fd->len = 0;
    fd->rows = NULL;

    char* line = NULL;
    size_t linecap = 0;
    ssize_t linelen;
    while((linelen = getline(&line, &linecap, fp)) != -1) 
    {
        while(linelen > 0 && (line[linelen - 1] == '\n' || line[linelen - 1] == '\r')) linelen--;

        erow* tmp = realloc(fd->rows, (fd->len + 1) * sizeof(erow));
        if(tmp == NULL) die("realloc rows");
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

/** editor operations **/
void editorInit(char* fl)
{
    e.x = 0;
    e.y = 0;
    e.nrows = getmaxy(stdscr);
    e.ncols = getmaxx(stdscr);
    e.of = 0;
    e.statusmsg[0] = '\0';
    strcpy(e.fl, fl);
    e.fd = openFile(fl);
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

void editorDelChar(erow* row, int at)
{
    if(at < 0 || at >= row->length) return;
    memmove(&row->data[at], &row->data[at + 1], row->length - at);
    row->length--;
}

void editorSetStatusMessage(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(e.statusmsg, sizeof(e.statusmsg), fmt, ap);
    va_end(ap);
}

/** input **/
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
            e.y = nrows - 2;
            break;
        case '/':
            e.of = 0;
            e.y = 0;
            break;
        default:
            return;
    }

    if(e.fd->len > 0) {
        if(e.x > e.fd->rows[e.y + e.of].length - 1) e.x = e.fd->rows[e.y + e.of].length - 1;
    }

    move(e.y, e.x);
}

void handleKeypress(int c)
{
    if(c == KEY_DOWN || c == KEY_UP || c == KEY_RIGHT || c == KEY_LEFT) moveCursor(c);
    else if(c == CTRL('j')) moveCursor('.');
    else if(c == CTRL('k')) moveCursor('/');
    else if(c == CTRL('e')) {editFlag = !editFlag; editorSetStatusMessage("EDIT MODE %s", editFlag ? "ON" : "OFF"); }
    else if(c == 127 && editFlag) editorDelChar(&e.fd->rows[e.y + e.of], e.x);
    else if(c > 31 && e.fd->len > 0 && editFlag) editorInsertChar(&e.fd->rows[e.y + e.of], e.x, c);
}

/** init **/
int main(int argc, char* argv[])
{
    initscr();
    noecho();
    raw();
    keypad(stdscr, TRUE);

    int nrows = getmaxy(stdscr);
    int ncols = getmaxx(stdscr);

    if(argc == 2) editorInit(argv[1]);
    else editorInit("NONE");

    editorSetStatusMessage("HELP: CTRL-Q to quit");

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