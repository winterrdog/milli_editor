#ifndef _MILLI_H
#define _MILLI_H 1

// formatting done by "webkit" clang-format:
// clang-format-11 -i -style="{BasedOnStyle: webkit, IndentWidth: 2}" *.c *.h
/***                                  INCLUDES                             ***/

// for portability
#define _DEFAULT_SOURCE 1
#define _GNU_SOURCE 1

#include <asm-generic/errno-base.h>
#include <asm-generic/ioctls.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

/***                                  DEFINES                             ***/
#define INT_PTR int32_t*
#define CHAR_PTR char*
#define CONST_CHAR_PTR const char*
#define BYTE unsigned char

// macro to clear the screen
#define CLR_SCRN()                      \
  {                                     \
    write(STDOUT_FILENO, "\x1b[2J", 4); \
    write(STDOUT_FILENO, "\x1b[H", 3);  \
  }

#define INIT_ARRAY(arr, init_val)                 \
  {                                               \
    for (u_int32_t x = 0; x < sizeof(arr); ++x) { \
      arr[x] = init_val;                          \
    }                                             \
  }

#define HANDLE_ERR(msg) \
  {                     \
    CLR_SCRN();         \
    perror(msg);        \
    exit(EXIT_FAILURE); \
  }

#define SAFE_FREE(x) \
  do {               \
    if (x) {         \
      free(x);       \
      x = NULL;      \
    }                \
  } while (0)

#define MILLI_VERSION "0.0.1"
#define MILLI_TAB_STOP 8
#define MILLI_QUIT_TIMES 3
#define STATUS_MSG_TIMEOUT 5
#define SEARCH_BACKWARDS -1
#define SEARCH_FORWARDS 1
#define SEARCH_NO_MATCH -1
#define HL_HIGHLIGHT_NUMBERS (1 << 0)
#define HL_HIGHLIGHT_STRINGS (1 << 1)
#define CTRL_KEY(key) ((key) & (0x1f))
#define HLDB_ENTRIES (sizeof(HLDB) / sizeof(HLDB[0]))

/***                                  DATA                                ***/

// struct containing all syntax highlighting information for a given file type
typedef struct editor_syntax {
  CHAR_PTR file_type;
  CHAR_PTR singleline_comment_start;
  CHAR_PTR multiline_comment_start;
  CHAR_PTR multiline_comment_end;
  CHAR_PTR* file_match;
  CHAR_PTR* keywords;
  int32_t flags;
} edt_sytx;

// struct to store rows of text
typedef struct editor_row {
  size_t size; // length of row in the file
  size_t rsize; // size of the contents of "render"
  BYTE* highlight; // contains highlight colors for lines in file
  int32_t index; // index of file row within the file
  int16_t hl_open_comment; // tracks rows in multi-line comments
  CHAR_PTR chars;
  CHAR_PTR render; // contains the actual characters to draw on the screen for
      // the current row of text
} edt_row;

struct editor_config {
  int32_t csr_x; // cursor's X position into "chars" content
  int32_t csr_y; // cursor's Y position
  int32_t render_x; // cursor's X position into "render" content
  int32_t row_off; // row offset to track scrolling into file
  int32_t col_off; // column offset to track scrolling into file
  int32_t dirty; // tracks if text buffer's dirty(if file's been modified)
  int32_t term_rows;
  int32_t term_cols;
  int32_t num_rows;
  u_int8_t empty_file;
  edt_row* row;
  CHAR_PTR fname;
  char status_msg[80];
  time_t status_msg_time;
  edt_sytx* syntax;
  struct termios
      orig_term_attrs; // storing the current state of the text editor
};

// special constants for arrow keys and other "escape" sequence characters
enum editorKey {
  BACKSPACE = 127,
  ARROW_LEFT = 1000,
  ARROW_RIGHT,
  ARROW_UP,
  ARROW_DOWN,
  DEL_KEY,
  HOME_KEY,
  END_KEY,
  PAGE_UP,
  PAGE_DOWN
};

// Possible highlight color values to use in our editor
enum editorHighlight {
  HL_NORMAL = 0,
  HL_COMMENT,
  HL_MLCOMMENT,
  HL_KEYWORD1,
  HL_KEYWORD2,
  HL_STRING,
  HL_NUMBER,
  HL_MATCH
};

/***                                APPEND BUFFER                          ***/

// dynamic string for appending only
struct abuf {
  CHAR_PTR buffer;
  size_t len;
};

#define ABUF_INIT \
  {               \
    NULL, 0       \
  }

/***                                  FUNCTION PROTOTYPES                 ***/
void disableRawMode(void);
void enableRawMode(void);
int32_t
editorReadKey(void);
int32_t
getCursorPosition(INT_PTR rows, INT_PTR cols);
int32_t
getTermWinSize(INT_PTR rows, INT_PTR cols);
int8_t
is_separator(int32_t ch);
void editorUpdateSyntax(edt_row* row);
int32_t
editorSyntaxToColor(int32_t hl_value);
void editorSelectSyntaxHighlight(void);
int32_t
editorRowCxToRx(edt_row* row, int32_t cx);
int32_t
editorRowRxToCx(edt_row* row, int32_t rx);
void editorUpdateRow(edt_row* row);
void editorInsertRow(int32_t at, CHAR_PTR s, size_t len);
void editorFreeRow(edt_row* row);
void editorDelRow(int32_t at);
void editorRowInsertChar(edt_row* row, int32_t at, int32_t ch);
void editorRowAppendStr(edt_row* row, CHAR_PTR s, size_t len);
void editorRowDelChar(edt_row* row, int32_t at);
void editorInsertChar(int32_t ch);
void editorInsertNewLine(void);
void editorDelChar(void);
CHAR_PTR
editorRowsToStr(INT_PTR buf_len);
void editorOpen();
void editorSave(void);
void editorFindCallback(CHAR_PTR query, int32_t key);
void editorFind(void);
void abAppend(struct abuf* ab, CONST_CHAR_PTR s, size_t len);
void abFree(struct abuf* ab);
void editorRefreshScreen(void);
CHAR_PTR
editorPrompt(CHAR_PTR prompt, void (*callback)(CHAR_PTR, int32_t));
void editorMoveCursor(int32_t key);
void editorProcessKeypress(void);
void editorScroll(void);
void editorDrawRows(struct abuf* ab);
void editorDrawStatusBar(struct abuf* ab);
void editorDrawMsgBar(struct abuf* ab);
void editorSetStatusMessage(CONST_CHAR_PTR fmt, ...);

void initEditor(void);

#endif