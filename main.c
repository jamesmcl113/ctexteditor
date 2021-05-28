#include <ncurses.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>

#define CTRL(c) ((c) & 0x1f)

#define VERSION "0.0.1"

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
    int hof;
    int vof;
    int eflag;
    char fl[128];
    char statusmsg[64];
    FileData* fd;
} Editor;

Editor e;

void die(const char *s)
{
    endwin();
    perror(s);
    exit(1);
}

/** buffer **/
typedef struct {
    char* data;
    int len;
} buf;

#define BUF_INIT {NULL, 0}

void displayBuf(buf* b)
{
    move(0, 0);
    for(int i = 0; i < b->len; i++)
    {
        char c = b->data[i];
        if(isdigit(c))
        {
            start_color();
            init_pair(1, COLOR_RED, -1);
            attron(COLOR_PAIR(1));
            addch(c);
            attroff(COLOR_PAIR(1));
            use_default_colors();
        } else if(c == '\'' || c == '"' || c == ';')
        {
            start_color();
            init_pair(2, COLOR_CYAN, -1);
            attron(COLOR_PAIR(2));
            addch(c);
            attroff(COLOR_PAIR(2));
            use_default_colors();
        }
        else addch(c);
    }
    refresh();
    //mvprintw(0, 0, b->data);

}

void appendBuf(buf* b, const char* str, int len)
{
    char* new = realloc(b->data, b->len + len);
    if(new == NULL) die("realloc buf");

    memcpy(&new[b->len], str, len);
    b->data = new;
    b->len += len;
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
        if(i == e.nrows / 3 && !strcmp(e.fl, "NONE"))
        {
            char msg[80];
            sprintf(msg, "Editor ---- version %s\n", VERSION);
            int padding = (e.ncols - strlen(msg)) / 2;
            if(padding)
            {
                appendBuf(b, "~", 1);
                padding--;
            }
            while(padding--) appendBuf(b, " ", 1);
            appendBuf(b, msg, strlen(msg));
        } else if(i < e.fd->len) {
            int len = e.fd->rows[i + e.vof].length - e.hof;
            if(len < 0) len = 0;
            if(len > e.ncols) len = e.ncols - 1;
            appendBuf(b, &e.fd->rows[i + e.vof].data[e.hof], len);
            appendBuf(b, "\n", 1);
        }  else {
            appendBuf(b, "~\n", 2);
        }
    }
}

void drawStatusLine()
{
    attrset(A_BOLD);
    move(e.nrows - 1, 0);
    clrtoeol();
    mvprintw(e.nrows - 1, 0, "> [%s] - %d lines", e.fl, e.fd->len);
    mvprintw(e.nrows - 1, e.ncols / 2, "%4d , %d", e.y + e.vof + 1, e.x + 1);
    mvprintw(e.nrows - 1, e.ncols - 35, "%s", e.statusmsg);
    attrset(A_NORMAL);
}

void refreshScreen()
{
    buf b = BUF_INIT;

    // get row data and output to screen
    drawRows(&b);
    displayBuf(&b);

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

FileData* newEmptyFile()
{
    FileData* fd = malloc(sizeof *fd);
    if(fd == NULL) die("malloc filedata");

    fd->len = 1;
    fd->rows = malloc(fd->len * sizeof(erow));

    erow* r = &fd->rows[0];
    r->data = malloc(2);
    r->data[0] = ' ';
    r->data[1] = '\0';
    r->length = 1;

    return fd;
}

void writeToFile()
{
    FILE* fp = fopen(e.fl, "w");

    if(e.fd->len == 0)
    {
        fprintf(fp, "");
        fclose(fp);
        return;
    }

    if(fp == NULL) die("couldn't open file!");

    for(int i = 0; i < e.fd->len; i++)
    {
        fprintf(fp, "%s\n", e.fd->rows[i].data);
    }

    fclose(fp);
}

/** editor operations **/
void editorInit(char* fl)
{
    e.x = 0;
    e.y = 0;
    e.nrows = getmaxy(stdscr);
    e.ncols = getmaxx(stdscr);
    e.hof = 0;
    e.vof = 0;
    e.eflag = 0;
    e.statusmsg[0] = '\0';
    strcpy(e.fl, fl);
    e.fd = openFile(fl);
}

int editorReadKey() {
    int c = getch();
    /*
    clrtoeol();
    mvprintw(0, 0, "%c", c);
    getch();
    */
    return c;
}

void editorMoveCursor(int c)
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
                if(e.vof > 0) e.vof--;
            } else e.y--;
            break;
        case KEY_DOWN:
            if(e.y + e.vof == e.fd->len - 1) ;
            else if(e.y == nrows - 2 && e.y + e.vof < e.fd->len) e.vof++;
            else if(e.y == nrows - 2 && e.fd->len < nrows - 2) ;
            else e.y++;
            break;
        case KEY_LEFT:
            if(e.fd->len > 0) {
                if(e.x <= 0)
                { 
                    e.x = 0;
                    if(e.hof > 0) e.hof--;
                    else if(e.y > 0) 
                    { 
                        e.y--;
                        e.x = e.fd->rows[e.y + e.vof].length - 1;
                        if(e.x > ncols - 1)
                        {
                            e.hof = e.fd->rows[e.y + e.vof].length - ncols;
                            e.x = ncols - 1; 
                        }
                    } 
                } else e.x--;
            }
            break;
        case KEY_RIGHT:
            if(e.fd->len > 0) {
                if(e.x + e.hof == e.fd->rows[e.y + e.vof].length - 1
                    && e.y + e.vof < e.fd->len - 1)
                {
                    e.y++;
                    e.x = 0;
                    e.hof = 0;
                } else if(e.x + e.hof == e.fd->rows[e.y + e.vof].length - 1) ;
                else if(e.x == ncols - 1) e.hof++;
                else if(e.x < e.fd->rows[e.y + e.vof].length) e.x++;
            }
            break;
        case '.':
            e.vof = maxOffset;
            e.y = nrows - 2 > e.fd->len ? e.fd->len - 1 : nrows - 2;
            break;
        case '/':
            e.vof = 0;
            e.y = 0;
            break;
        default:
            return;
    }
    
    // change position/offset of cursor if we've moved to a short line
    if(e.fd->len > 0) {
        if(e.x + e.hof > e.fd->rows[e.y + e.vof].length) {
            e.x = e.fd->rows[e.y + e.vof].length - 1;
            if(e.fd->rows[e.y + e.vof].length < ncols) e.hof = 0;
        }
    }

    move(e.y, e.x);
}

void editorInsertChar(erow* row, int at, char c)
{
    if(at < 0 || at > row->length) at = row->length;

    row->data = realloc(row->data, row->length + 2);
    memmove(&row->data[at + 1], &row->data[at],
            row->length - at + 1);
    row->length++;
    row->data[at] = c;
    row->data[row->length] = '\0';
}   

void editorDelChar(erow* row, int at)
{
    if(at < 0 || at >= row->length) return;
    memmove(&row->data[at], &row->data[at + 1], row->length - at);
    row->length--;
    editorMoveCursor(KEY_LEFT);
}

void editorSetStatusMessage(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(e.statusmsg, sizeof(e.statusmsg), fmt, ap);
    va_end(ap);
}

char* editorPrompt(char* prompt)
{
    size_t bufsize = 128;
    char* buf = malloc(bufsize);

    size_t buflen = 0;
    buf[0] = '\0';

    while(1)
    {
        editorSetStatusMessage(prompt, buf);
        refreshScreen();

        int c = editorReadKey();
        if(c == '\n' || c == '\r')
        {
            if(buflen != 0)
            {
                editorSetStatusMessage("");
                return buf;
            }
        } else if (!iscntrl(c) && c < 128)
        {
            if(buflen == bufsize - 1)
            {
                bufsize *= 2;
                buf = realloc(buf, bufsize);
            }
            buf[buflen++] = c;
            buf[buflen] = '\0';
        }
    }
}

void editorCreateFile(char* fn)
{
    FileData* fd = newEmptyFile();
    strcpy(e.fl, fn);
    e.fd = fd;
    //editorInsertChar(&e.fd->rows[0], 0, ' ');
    refreshScreen();
    return;
}

void editorFreeRow(erow* row)
{
    free(row->data);
    row->length = 0;
}

void editorDelRow(int at)
{
    if(at < 0 || at >= e.fd->len) return;

    editorFreeRow(&e.fd->rows[at]);
    memmove(&e.fd->rows[at], &e.fd->rows[at + 1], 
            sizeof(erow) * (e.fd->len - at - 1));
    e.fd->len--;
}

void editorAppendStringToRow(int at, char* s, size_t len)
{
    if(at < 0 || at >= e.fd->len) return;

    erow* r = &e.fd->rows[at];
    r->data = realloc(r->data, r->length + len + 1);
    memcpy(&r->data[r->length], s, len);
    r->data[len + r->length] = '\0';
    r->length += len;
}

void editorAddRow(int at, char* s, size_t len)
{
    if(at < 0 || at >= e.fd->len) at = 0;

    e.fd->rows = realloc(e.fd->rows, (e.fd->len + 1) * sizeof(erow));
    memmove(&e.fd->rows[at + 1], &e.fd->rows[at], (e.fd->len - at) * sizeof(erow));

    erow* r = &e.fd->rows[at];
    r->length = len;
    r->data = malloc(len + 1);
    memcpy(r->data, s, len);
    r->data[len] = '\0';

    e.fd->len++;
}  

void editorFree()
{
    for(int i = 0; i < e.fd->len; i++) editorFreeRow(&e.fd->rows[i]);
    free(e.fd->rows);
    free(e.fd);
}

/** input **/
int handleKeypress()
{
    int c = editorReadKey();
    switch(c)
    {
        case CTRL('q'):
            return 1;
            break;
        
        case KEY_DOWN:
        case KEY_LEFT:
        case KEY_UP:
        case KEY_RIGHT:
            editorMoveCursor(c);
            break;
        case CTRL('l'):
            editorMoveCursor('.');
            break;
        case CTRL('k'):
            //editorMoveCursor('/');
            editorAppendStringToRow(0, "sandwich", 8);
            break;
        
        case CTRL('a'):
            if(!e.eflag) editorMoveCursor(KEY_RIGHT);
        case CTRL('e'):
            if(!strcmp(e.fl, "NONE")) editorCreateFile("empty.txt"); 
            // we should really add a new line here, add some variable to check
            // if we started by opening a file or are creating a brand new one
            // also create a switchEditMode function

            e.eflag = !e.eflag; 
            editorSetStatusMessage("EDIT MODE %s", e.eflag ? "ON" : "OFF");
            break;

        case CTRL('d'):
            editorDelRow(e.y + e.vof);
            break;

        case CTRL('o'):
            editorAddRow(e.y + e.vof, " ", 1);
            e.eflag = 1;
            editorSetStatusMessage("EDIT MODE %s", e.eflag ? "ON" : "OFF");
            break;
        
        case CTRL('s'):
            writeToFile();
            editorSetStatusMessage("Saved %s!", e.fl);
            break;

        case 127:
            if(e.eflag) 
            {
                if(e.x == 0 && e.y > 0) 
                {   
                    editorMoveCursor(KEY_LEFT);
                    editorAppendStringToRow(e.y + e.vof, e.fd->rows[e.y + e.vof + 1].data, e.fd->rows[e.y + e.vof + 1].length);
                    editorDelRow(e.y + e.vof + 1);
                }
                else editorDelChar(&e.fd->rows[e.y + e.vof], e.x - 1);
            }
            break;

        case '\n':
            break;
        
        default:
            if(c > 31 && e.fd->len > 0 && e.eflag)
            {
                //if(e.x == 0) editorInsertChar(&e.fd->rows[e.y + e.vof], e.x, ' ');
                editorInsertChar(&e.fd->rows[e.y + e.vof], e.x, c);
                editorMoveCursor(KEY_RIGHT);
            }
            break;
        
    }
    return 0;
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

    int q;
    refreshScreen();
    while(1)
    {
        q = handleKeypress();
        refreshScreen();
        if(q) break;
    }
    
    editorFree();
    endwin();
    return 0;
}