/***                                  INCLUDES                             ***/

#include "milli.h"

/***                                  GLOBAL DATA                          ***/

struct editor_config edt_conf;

/***                                FILETYPES                              ***/
CHAR_PTR C_HL_extensions[] = { ".c", ".cpp", ".h", ".hpp", NULL };

// secondary keywords end in a "|" unlike primary ones
CHAR_PTR C_HL_keywords[] = {
  "switch", "while", "for", "if", "break", "continue",
  "return", "else", "struct", "union", "typedef", "static",
  "extern", "enum", "class", "case", "register", "auto",
  "do", "default", "goto", "inline", "restrict", "sizeof",
  "volatile", "const", "int|", "float|", "double|", "long|",
  "char|", "unsigned|", "signed|", "void|", "short|", "int32_t|",
  "int8_t|", "u_int8_t|", "int16_t|", "u_int16_t|", "u_int32_t|", "edt_row|",
  "time_t|", "edt_sytx|", "size_t|", "FILE|", NULL
};

edt_sytx HLDB[] = {
  { "c", "//", "/*", "*/", C_HL_extensions, C_HL_keywords,
      HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS },
};

/***                                TERMINAL                              ***/

// Used for disabling raw mode
void disableRawMode(void)
{
  if (-1 == tcsetattr(STDIN_FILENO, TCSAFLUSH, &edt_conf.orig_term_attrs)) {
    HANDLE_ERR("tcsetattr");
  }
}

// Change terminal mode from cooked mode to raw mode by
// changing the default terminal attributes
void enableRawMode(void)
{
  if (-1 == tcgetattr(STDIN_FILENO, &edt_conf.orig_term_attrs)) {
    HANDLE_ERR("tcgetattr");
  }

  atexit(disableRawMode);

  // alter terminal attributes to turn off "input echoing", "canonical/cooked
  // mode", "software flow control(Ctrl-S / Ctrl-Q)", "Ctrl-V", "Ctrl-M",
  // "Output processing" and "Ctrl-Z / Ctrl-C"
  struct termios raw = edt_conf.orig_term_attrs;
  raw.c_iflag = raw.c_iflag & ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
  raw.c_oflag = raw.c_oflag & ~(OPOST);
  raw.c_cflag = raw.c_cflag | CS8; // set character size to 8bits.
  raw.c_lflag = raw.c_lflag & ~(ECHO | ICANON | ISIG | IEXTEN);
  raw.c_cc[VMIN] = 0; // min no of read characters
  raw.c_cc[VTIME] = 1; // timeout for reading in milli-secs

  if (-1 == tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw)) {
    HANDLE_ERR("tcsetattr");
  }
}

// Reads and returns input keypresses from user
int32_t editorReadKey(void)
{
  int32_t nchar_read = 0;
  char in_key = '\0';

  // keep reading till u get character
  for (;;) {
    nchar_read = read(STDIN_FILENO, &in_key, 1);

    switch ((nchar_read)) {
    case -1:
      if (errno != EAGAIN) {
        HANDLE_ERR("read")
      } else {
        continue;
      }

    case 1:
      if (in_key == '\x1b') { // special keys
        char seq[3] = { '\0' };

        if (read(STDIN_FILENO, seq, 1) != 1) {
          return '\x1b';
        }

        if (read(STDIN_FILENO, (seq + 1), 1) != 1) {
          return '\x1b';
        }

        if (seq[0] == '[') {
          if (seq[1] >= '0' && seq[1] <= '9') {
            if (read(STDIN_FILENO, (seq + 2), 1) != 1) {
              return '\x1b';
            }

            // Handle HOME, ENDPAGE UP and DOWN keys
            if (seq[2] == '~') {
              switch (seq[1]) {
              case '1':
                return HOME_KEY;
              case '3':
                return DEL_KEY;
              case '4':
                return END_KEY;
              case '5':
                return PAGE_UP;
              case '6':
                return PAGE_DOWN;
              case '7':
                return HOME_KEY;
              case '8':
                return END_KEY;
              }
            }
          } else {
            // Handle arrow keys
            switch (seq[1]) {
            case 'A':
              return ARROW_UP;
            case 'B':
              return ARROW_DOWN;
            case 'C':
              return ARROW_RIGHT;
            case 'D':
              return ARROW_LEFT;
            case 'H':
              return HOME_KEY;
            case 'F':
              return END_KEY;
            }
          }
        } else if (seq[0] == 'O') {
          switch (seq[1]) {
          case 'H':
            return HOME_KEY;
          case 'F':
            return END_KEY;
          }
        }

        return '\x1b';
      } else {
        // other non-special keys
        return in_key;
      }
    }
  }
}

// Gets the current cursor position in the terminal interface
int32_t getCursorPosition(INT_PTR rows, INT_PTR cols)
{
  char buf[32] = { '\0' };
  u_int32_t i = 0;

  // query cursor position using the "N" command.
  if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) {
    return -1;
  }

  for (; i < sizeof(buf) - 1; ++i) {
    if ((read(STDIN_FILENO, &buf[i], 1) != 1) || buf[i] == 'R') {
      break;
    }
  }

  // make sure it's an "escape" sequence
  if (buf[0] != '\x1b' || buf[1] != '[') {
    return -1;
  }

  // the 'N' command returns sth like this: "&buf[1]: '\x1b[43;126R'" but
  // we only need the integers(rows & columns) therein.
  if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) {
    return -1;
  }

  printf("\r\n&buf[1]: '%s'\r\n", &buf[1]);

  return 0;
}

// Gets the terminal window interface's dimensions
int32_t getTermWinSize(INT_PTR rows, INT_PTR cols)
{
  struct winsize ws = { 0, 0, 0, 0 };

  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    // query cursor position in a "portable" way. The "C" cmd moves cursor
    // forwards 2 the right and "B" cmd moves the cursor down to the bottom
    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) {
      return -1;
    }

    return getCursorPosition(rows, cols);
  }

  // store terminal dimensions
  *cols = ws.ws_col;
  *rows = ws.ws_row;

  return 0;
}

/***                                SYNTAX HIGHLIGHTING                    ***/

// Checks if a character is a separator
int8_t is_separator(int32_t ch)
{
  return isspace(ch) || ch == '\0' || strchr("{}'\",.()+-/*=~%%<>[];", ch) != NULL;
}

// Sets the highlight color for each character in a row
void editorUpdateSyntax(edt_row* row)
{
  row->highlight = realloc(row->highlight, row->rsize);
  memset(row->highlight, HL_NORMAL, row->rsize);

  // if there's no filetype set
  if (!edt_conf.syntax) {
    return;
  }

  CHAR_PTR* keywords = edt_conf.syntax->keywords;

  CHAR_PTR sl_comm = edt_conf.syntax->singleline_comment_start;
  CHAR_PTR mc_start = edt_conf.syntax->multiline_comment_start;
  CHAR_PTR mc_end = edt_conf.syntax->multiline_comment_end;

  int8_t sl_comm_len = sl_comm ? strlen(sl_comm) : 0;
  int8_t mc_start_len = mc_start ? strlen(mc_start) : 0;
  int8_t mc_end_len = mc_end ? strlen(mc_end) : 0;

  int16_t prev_sep = 1;
  int8_t in_string = 0;
  int8_t in_ml_comm = (row->index > 0 && edt_conf.row[row->index - 1].hl_open_comment);

  size_t i = 0;
  while (i < row->rsize) {
    char ch = row->render[i];
    BYTE prev_highlight = (i > 0) ? row->highlight[i - 1] : HL_NORMAL;

    // highlight single-line comments
    if (sl_comm_len && !in_string && !in_ml_comm) {
      if (!strncmp(row->render + i, sl_comm, sl_comm_len)) {
        memset(row->highlight + i, HL_COMMENT, row->rsize - i);
        break;
      }
    }

    // highlight multi-line comments
    if (mc_start_len && mc_end_len && !in_string) {
      if (in_ml_comm) {
        row->highlight[i] = HL_MLCOMMENT;
        if (!strncmp(row->render + i, mc_end, mc_end_len)) {
          memset(row->highlight + i, HL_MLCOMMENT, mc_end_len);
          i += mc_end_len;
          in_ml_comm = 0;
          prev_sep = 1;
          continue;
        } else {
          ++i;
          continue;
        }
      } else if (!strncmp(row->render + i, mc_start, mc_start_len)) {
        memset(row->highlight + i, HL_MLCOMMENT, mc_start_len);
        i += mc_start_len;
        in_ml_comm = 1;
        continue;
      }
    }

    // highlight strings
    if (edt_conf.syntax->flags & HL_HIGHLIGHT_STRINGS) {
      if (in_string) {
        row->highlight[i] = HL_STRING;

        if (ch == '\\' && (i + 1) < row->rsize) {
          row->highlight[i + 1] = HL_STRING;
          i += 2;
          continue;
        }

        if (ch == in_string) {
          in_string = 0;
        }

        ++i;
        prev_sep = 1;
        continue;
      } else {
        if (ch == '"' || ch == '\'') {
          in_string = ch;
          row->highlight[i] = HL_STRING;
          ++i;
          continue;
        }
      }
    }

    // highlight numbers
    if (edt_conf.syntax->flags & HL_HIGHLIGHT_NUMBERS) {
      if ((isdigit(ch) && (prev_sep || prev_highlight == HL_NUMBER)) || (ch == '.' && prev_highlight == HL_NUMBER)) {
        row->highlight[i] = HL_NUMBER;
        ++i;
        prev_sep = 0;
        continue;
      }
    }

    // highlight keywords
    if (prev_sep) {
      u_int32_t j = 0;
      for (; keywords[j]; ++j) {
        size_t keywd_len = strlen(keywords[j]);
        int32_t keywd_2 = keywords[j][keywd_len - 1] == '|';

        if (keywd_2) {
          --keywd_len;
        }

        if (!strncmp(row->render + i, keywords[j], keywd_len) && is_separator(row->render[i + keywd_len])) {
          memset(row->highlight + i, keywd_2 ? HL_KEYWORD2 : HL_KEYWORD1,
              keywd_len);
          i += keywd_len;
          break;
        }
      }

      if (keywords[j]) {
        prev_sep = 0;
        continue;
      }
    }

    prev_sep = is_separator(ch);
    ++i;
  }

  u_int8_t changed = (row->hl_open_comment != in_ml_comm);
  row->hl_open_comment = in_ml_comm;
  if (changed && row->index + 1 < edt_conf.num_rows) {
    editorUpdateSyntax(edt_conf.row + (row->index + 1));
  }
}

// Maps the editor's possible highlight values to their corresponding ASCII
// color codes
int32_t editorSyntaxToColor(int32_t hl_value)
{
  switch (hl_value) {
  case HL_NUMBER:
    return 31;
  case HL_KEYWORD2:
    return 32;
  case HL_KEYWORD1:
    return 33;
  case HL_MATCH:
    return 34;
  case HL_STRING:
    return 35;
  case HL_COMMENT:
  case HL_MLCOMMENT:
    return 36;
  default:
    return 37;
  }
}

// Matches current filename to their respective syntax highlighting
void editorSelectSyntaxHighlight(void)
{
  edt_conf.syntax = NULL;
  if (!edt_conf.fname) {
    return;
  }

  CHAR_PTR ext = strchr(edt_conf.fname, '.');
  for (u_int32_t j = 0; j < HLDB_ENTRIES; ++j) {
    edt_sytx* sytx = HLDB + j;

    u_int32_t i = 0;
    while (sytx->file_match[i]) {
      int32_t is_ext = (sytx->file_match[i][0] == '.');
      if ((is_ext && ext && !strcmp(ext, sytx->file_match[i])) || (!is_ext && strstr(edt_conf.fname, sytx->file_match[i]))) {
        edt_conf.syntax = sytx;

        // rehighlight file after setting the syntax highlighting
        int32_t file_row = 0;
        for (; file_row < edt_conf.num_rows;
             editorUpdateSyntax(edt_conf.row + file_row), ++file_row)
          ;

        return;
      }
      ++i;
    }
  }
}

/***                                ROW OPERATIONS                         ***/

int32_t editorRowCxToRx(edt_row* row, int32_t cx)
{
  int32_t rx = 0x0;
  int32_t j = 0x0;
  for (; j < cx; ++j, ++rx) {
    if (row->chars[j] == '\t') {
      rx += (MILLI_TAB_STOP - 1) - (rx % MILLI_TAB_STOP);
    }
  }

  return rx;
}

int32_t editorRowRxToCx(edt_row* row, int32_t rx)
{
  int32_t cur_rx = 0x0;
  int32_t csr_x = 0x0;
  for (; (size_t)csr_x < row->size; ++csr_x) {
    if (row->chars[csr_x] == '\t') {
      cur_rx += (MILLI_TAB_STOP - 1) - (cur_rx % MILLI_TAB_STOP);
    }

    ++cur_rx;

    if (cur_rx > rx) {
      return csr_x;
    }
  }

  return csr_x;
}

void editorUpdateRow(edt_row* row)
{
  u_int16_t tabs = 0x0;
  size_t j = 0x0;
  for (; j < row->size; ++j) {
    if (row->chars[j] == '\t') {
      ++tabs;
    }
  }

  if (row->render) {
    free(row->render);
  }

  row->render = malloc(row->size + (tabs * (MILLI_TAB_STOP - 1)) + 1);

  size_t index = 0;
  for (j = 0; j < row->size; ++j) {
    // rendering tabs
    if (row->chars[j] == '\t') {
      row->render[index++] = ' ';

      // print spaces till a tab-stop
      while (index % MILLI_TAB_STOP != 0x0) {
        row->render[index++] = ' ';
      }
    } else {
      row->render[index++] = row->chars[j];
    }
  }

  row->render[index] = '\0';
  row->rsize = index;

  editorUpdateSyntax(row);
}

void editorInsertRow(int32_t at, CHAR_PTR s, size_t len)
{
  if (at < 0 || at > edt_conf.num_rows) {
    return;
  }

  edt_conf.row = realloc(edt_conf.row, sizeof(edt_row) * (edt_conf.num_rows + 1));
  memmove(edt_conf.row + (at + 1), edt_conf.row + at,
      sizeof(edt_row) * (edt_conf.num_rows - at));

  int32_t j = at + 1;
  for (; j < edt_conf.num_rows; ++edt_conf.row[j].index, ++j)
    ;

  edt_conf.row[at].index = at;

  // store new row, s, into our editor's row buffer
  edt_conf.row[at].size = len;
  edt_conf.row[at].chars = calloc(len + 1, sizeof(char));
  memcpy(edt_conf.row[at].chars, s, len);

  edt_conf.row[at].chars[len] = '\0';

  edt_conf.row[at].rsize = 0x0;
  edt_conf.row[at].render = NULL;
  edt_conf.row[at].highlight = NULL;
  edt_conf.row[at].hl_open_comment = 0x0;
  editorUpdateRow(edt_conf.row + at);

  ++edt_conf.num_rows;
  ++edt_conf.dirty;
}

void editorFreeRow(edt_row* row)
{
  if (row) {
    for (int32_t i = 0; i < edt_conf.num_rows; ++i) {
      edt_row* tmp_row = row + i;

      if (tmp_row->chars && tmp_row->render && tmp_row->highlight) {
        free(tmp_row->render);
        free(tmp_row->chars);
        free(tmp_row->highlight);

        tmp_row->render = NULL;
        tmp_row->chars = NULL;
        tmp_row->highlight = NULL;
      }
    }

    free(row);
    row = NULL;
  }
}

void editorDelRow(int32_t at)
{
  if (at < 0 || at >= edt_conf.num_rows) {
    return;
  }

  for (int32_t j = at; j < edt_conf.num_rows - 1; ++j) {
    --edt_conf.row[j].index;
  }

  editorFreeRow(edt_conf.row + at);
  memmove(edt_conf.row + at, edt_conf.row + (at + 1),
      (sizeof(edt_row) * (edt_conf.num_rows - (at + 1))));
  --edt_conf.num_rows;
  ++edt_conf.dirty;
}

void editorRowInsertChar(edt_row* row, int32_t at, int32_t ch)
{
  // add characters to a line/row
  if (at < 0x0 || (size_t)at > row->size) {
    at = row->size;
  }

  row->chars = realloc(row->chars, row->size + 2);
  memmove((row->chars + (at + 1)), (row->chars + at), row->size - at + 1);

  ++row->size;
  row->chars[at] = ch;

  editorUpdateRow(row);
  ++edt_conf.dirty;
}

void editorRowAppendStr(edt_row* row, CHAR_PTR s, size_t len)
{
  row->chars = realloc(row->chars, row->size + len + 1);

  memcpy(row->chars + row->size, s, len);
  row->size += len;

  row->chars[row->size] = '\0';
  editorUpdateRow(row);
  ++edt_conf.dirty;
}

void editorRowDelChar(edt_row* row, int32_t at)
{
  if (at < 0 || (size_t)at >= row->size) {
    return;
  }

  memmove(row->chars + at, row->chars + (at + 1), row->size - at);

  --row->size;
  editorUpdateRow(row);
  ++edt_conf.dirty;
}

/***                                EDITOR OPERATIONS                      ***/
void editorInsertChar(int32_t ch)
{
  // add newline to EOF
  if (edt_conf.csr_y == edt_conf.num_rows) {
    editorInsertRow(edt_conf.num_rows, "", 0x0);
  }

  editorRowInsertChar(edt_conf.row + edt_conf.csr_y, edt_conf.csr_x, ch);

  ++edt_conf.csr_x;
}

void editorInsertNewLine(void)
{
  if (edt_conf.csr_x == 0) {
    editorInsertRow(edt_conf.csr_y, "", 0);
  } else {
    edt_row* row = edt_conf.row + edt_conf.csr_y;
    editorInsertRow(edt_conf.csr_y + 1, row->chars + edt_conf.csr_x,
        row->size - edt_conf.csr_x);

    row = edt_conf.row + edt_conf.csr_y;
    row->size = edt_conf.csr_x;
    row->chars[row->size] = '\0';

    editorUpdateRow(row);
  }

  ++edt_conf.csr_y;
  edt_conf.csr_x = 0;
}

void editorDelChar(void)
{
  if (edt_conf.csr_y == edt_conf.num_rows) {
    return;
  }

  if (edt_conf.csr_x == 0 && edt_conf.csr_y == 0) {
    return;
  }

  edt_row* row = edt_conf.row + edt_conf.csr_y;
  if (edt_conf.csr_x > 0) {
    editorRowDelChar(row, edt_conf.csr_x - 1);
    --edt_conf.csr_x;
  } else {
    edt_conf.csr_x = edt_conf.row[edt_conf.csr_y - 1].size;
    editorRowAppendStr(edt_conf.row + edt_conf.csr_y - 1, row->chars,
        row->size);
    editorDelRow(edt_conf.csr_y);
    --edt_conf.csr_y;
  }
}

/***                                FILE I/O                               ***/

CHAR_PTR editorRowsToStr(INT_PTR buf_len)
{
  u_int32_t tot_len = 0;
  u_int32_t j = 0;
  for (; j < (u_int32_t)edt_conf.num_rows; ++j) {
    tot_len += edt_conf.row[j].size + 1;
  }
  *buf_len = tot_len;

  CHAR_PTR buf = calloc(tot_len, sizeof(char));
  if (!buf) {
    return NULL;
  }

  CHAR_PTR p = buf;
  for (j = 0; j < (u_int32_t)edt_conf.num_rows; ++j, ++p) {
    memcpy(p, edt_conf.row[j].chars, edt_conf.row[j].size);
    p += edt_conf.row[j].size;
    *p = '\n';
  }

  return buf;
}

// Opens and reads a file from disk
void editorOpen()
{
  editorSelectSyntaxHighlight();

  // open file for reading
  FILE* fp = fopen(edt_conf.fname, "r");
  if (!fp) {
    HANDLE_ERR("fopen")
  }

  // read lines from the opened file
  CHAR_PTR line = NULL;
  size_t line_cap = 0;
  ssize_t line_len = 0;

  // register the read data into the editor for display
  while ((line_len = getline(&line, &line_cap, fp)) != -1) {
    // strip off newline or carriage return characters
    while (line_len > 0 && (line[line_len - 1] == '\n' || line[line_len - 1] == '\r')) {
      --line_len;
    }

    editorInsertRow(edt_conf.num_rows, line, line_len);
  }

  free(line);
  fclose(fp);

  edt_conf.dirty = 0;
}

void editorSave(void)
{
  if (!edt_conf.fname) {
    edt_conf.fname = editorPrompt("Save as: %s (ESC to cancel)", NULL);

    if (!edt_conf.fname) {
      editorSetStatusMessage("Save ABORTED!");
      return;
    }

    edt_conf.empty_file = 1;
    editorSelectSyntaxHighlight();
  }

  int32_t len = 0;
  CHAR_PTR buf = editorRowsToStr(&len);
  if (!buf) {
    return;
  }

  // open a file and write to disk
  int32_t fd = open(edt_conf.fname, O_RDWR | O_CREAT, 0644);
  if (fd != -1) {
    if (ftruncate(fd, len) != -1) {
      if (write(fd, buf, len) == len) {
        close(fd);
        free(buf);
        buf = NULL;

        edt_conf.dirty = 0;
        editorSetStatusMessage("%d bytes were written to DISK.", len);
        return;
      }
    }

    close(fd);
  }

  free(buf);
  buf = NULL;
  editorSetStatusMessage("Failed to save file. I/O error: %s", strerror(errno));
}

/***                                FIND                                   ***/

// Callback to locate search query.
void editorFindCallback(CHAR_PTR query, int32_t key)
{
  static int32_t last_match = SEARCH_NO_MATCH;
  static int8_t direction = SEARCH_FORWARDS;

  // restore the text highlight after a search
  static int32_t saved_hl_line = 0;
  static CHAR_PTR saved_hl = NULL;
  if (saved_hl) {
    memcpy(edt_conf.row[saved_hl_line].highlight, saved_hl,
        edt_conf.row[saved_hl_line].rsize);
    free(saved_hl);
    saved_hl = NULL;
  }

  if (key == '\r' || key == '\x1b') {
    last_match = SEARCH_NO_MATCH;
    direction = SEARCH_FORWARDS;
    return;
  } else if (key == ARROW_RIGHT || key == ARROW_DOWN) {
    direction = SEARCH_FORWARDS;
  } else if (key == ARROW_LEFT || key == ARROW_UP) {
    direction = SEARCH_BACKWARDS;
  } else {
    last_match = SEARCH_NO_MATCH;
    direction = SEARCH_FORWARDS;
  }

  // set cursor to the queried string's position
  if (last_match == SEARCH_NO_MATCH) {
    direction = SEARCH_FORWARDS;
  }

  int32_t curr_match_row = last_match;
  int32_t i = 0;
  for (; i < edt_conf.num_rows; ++i) {
    curr_match_row += direction;

    if (curr_match_row == SEARCH_NO_MATCH) {
      curr_match_row = edt_conf.num_rows - 1;
    } else if (curr_match_row == edt_conf.num_rows) {
      curr_match_row = 0;
    }

    edt_row* row = edt_conf.row + curr_match_row;
    CHAR_PTR match = strstr(row->render, query);
    if (match) {
      last_match = curr_match_row;
      edt_conf.csr_y = curr_match_row;
      edt_conf.csr_x = editorRowRxToCx(row, match - row->render);
      edt_conf.row_off = edt_conf.num_rows;

      saved_hl_line = curr_match_row;
      saved_hl = calloc(row->rsize, sizeof(char));
      memcpy(saved_hl, row->highlight, row->rsize);
      memset(row->highlight + (match - row->render), HL_MATCH, strlen(query));

      break;
    }
  }
}

void editorFind(void)
{
  // save cursor & scroll position before search
  int32_t saved_csr_x = edt_conf.csr_x;
  int32_t saved_csr_y = edt_conf.csr_y;
  int32_t saved_col_off = edt_conf.col_off;
  int32_t saved_row_off = edt_conf.row_off;

  // get search query from user
  CHAR_PTR query = editorPrompt(
      "Search: %s (ESC to cancel | ARROW keys to navigate | Enter key )",
      editorFindCallback);
  if (!query) {
    return;
  } else {
    // restore saved cursor & scroll position in case no query was input
    edt_conf.csr_x = saved_csr_x;
    edt_conf.csr_y = saved_csr_y;
    edt_conf.col_off = saved_col_off;
    edt_conf.row_off = saved_row_off;
  }

  free(query);
}

/***                                APPEND BUFFER                          ***/

// Appends data to a custom dynamic output screen buffer
void abAppend(struct abuf* ab, CONST_CHAR_PTR s, size_t len)
{
  CHAR_PTR new = realloc(ab->buffer, (ab->len + len));

  if (!new)
    return;

  memcpy((new + ab->len), s, len);

  ab->buffer = new;
  ab->len += len;
}

// Frees allocated memory for the output screen buffer
void abFree(struct abuf* ab)
{
  if (ab->buffer) {
    free(ab->buffer);
  }
}

/***                                INPUT                                  ***/

// Asks user for input in cases like saving files or passing commands
CHAR_PTR editorPrompt(CHAR_PTR prompt, void (*callback)(CHAR_PTR, int32_t))
{
  size_t bufsize = 128;
  CHAR_PTR buf = calloc(bufsize, sizeof(char));

  size_t buflen = 0;

  for (;;) {
    editorSetStatusMessage(prompt, buf);
    editorRefreshScreen();

    int32_t ch = editorReadKey();
    if (ch == DEL_KEY || ch == CTRL_KEY('h') || ch == BACKSPACE) {
      if (buflen != 0) {
        buf[--buflen] = '\0';
      }
    } else if (ch == '\x1b') {
      // using "ESC" to cancel user input
      editorSetStatusMessage("");
      if (callback) {
        callback(buf, ch);
      }

      free(buf);
      return NULL;
    } else if (ch == '\r') {
      if (buflen != 0) {
        editorSetStatusMessage("");
        if (callback) {
          callback(buf, ch);
        }

        return buf;
      }
    } else if (!iscntrl(ch) && ch < 128) {
      if (buflen == bufsize - 1) {
        bufsize *= 2;
        buf = realloc(buf, bufsize);
      }

      buf[buflen++] = ch;
      buf[buflen] = '\0';
    }

    if (callback) {
      callback(buf, ch);
    }
  }
}

// Moves the cursor using ARROW keys
void editorMoveCursor(int32_t key)
{
  edt_row* row = (edt_conf.csr_y >= edt_conf.num_rows)
      ? NULL
      : edt_conf.row + edt_conf.csr_y;
  switch (key) {
  case ARROW_LEFT:
    // bounds checking to prevent the cursor exceeding its bounds
    if (edt_conf.csr_x != 0) {
      --edt_conf.csr_x;
    } else if (edt_conf.csr_y > 0) {
      // move cursor to the end of previous line when arrow left is pressed at
      // the beginning of a line
      --edt_conf.csr_y;
      edt_conf.csr_x = edt_conf.row[edt_conf.csr_y].size;
    }
    break;
  case ARROW_RIGHT:
    // limit scrolling to the right
    if (row && (size_t)edt_conf.csr_x < row->size) {
      ++edt_conf.csr_x;
    } else if (row && (size_t)edt_conf.csr_x == row->size) {
      // move cursor to the start of next line when arrow right is pressed at
      // the end of a line
      ++edt_conf.csr_y;
      edt_conf.csr_x = 0x0;
    }
    break;
  case ARROW_UP:
    // bounds checking to prevent the cursor exceeding its bounds
    if (edt_conf.csr_y != 0) {
      --edt_conf.csr_y;
    }
    break;
  case ARROW_DOWN:
    // bounds checking to prevent the cursor exceeding its bounds
    if (edt_conf.csr_y < edt_conf.num_rows) {
      ++edt_conf.csr_y;
    }
    break;
  }

  // snap cursor to end of line
  row = (edt_conf.csr_y >= edt_conf.num_rows) ? NULL
                                              : edt_conf.row + edt_conf.csr_y;

  int32_t row_len = row ? row->size : 0x0;
  if (edt_conf.csr_x > row_len) {
    edt_conf.csr_x = row_len;
  }
}

// Reads the user-typed/input keys and executes actions for them.
void editorProcessKeypress(void)
{
  static u_int8_t quit_times = MILLI_QUIT_TIMES;
  int32_t in_key = editorReadKey();

  switch (in_key) {
  case '\r':
    editorInsertNewLine();
    break;
  case CTRL_KEY('q'): // use Ctrl-Q to quit
    if (edt_conf.dirty && quit_times > 0) {
      editorSetStatusMessage("WARNING! file has unsaved changes. Press "
                             "Ctrl-Q %d more times to force-quit.",
          quit_times--);

      return;
    }

    // Memory clean up
    editorFreeRow(edt_conf.row);

    if (edt_conf.fname && edt_conf.empty_file) {
      free(edt_conf.fname);
      edt_conf.fname = NULL;
    }

    CLR_SCRN();
    exit(EXIT_SUCCESS);
    break;

  case CTRL_KEY('s'):
    // save a file
    editorSave();
    break;

  case CTRL_KEY('f'):
    // Find a string in file
    editorFind();
    break;

  case HOME_KEY:
    edt_conf.csr_x = 0;
    break;

  case END_KEY:
    // move cursor to end of line
    if (edt_conf.csr_y < edt_conf.num_rows) {
      edt_conf.csr_x = edt_conf.row[edt_conf.csr_y].size;
    }
    break;

  case BACKSPACE:
  case CTRL_KEY('h'):
  case DEL_KEY:
    if (in_key == DEL_KEY) {
      editorMoveCursor(ARROW_RIGHT);
    }
    editorDelChar();
    break;

  case PAGE_UP:
  case PAGE_DOWN: {
    // scrolling entire pages with PAGE up and down keys
    if (in_key == PAGE_UP) {
      edt_conf.csr_y = edt_conf.row_off;
    } else if (in_key == PAGE_DOWN) {
      edt_conf.csr_y = edt_conf.row_off + edt_conf.term_rows - 1;

      if (edt_conf.csr_y > edt_conf.num_rows) {
        edt_conf.csr_y = edt_conf.num_rows;
      }
    }

    int32_t times = edt_conf.term_rows;

    while (times--) {
      editorMoveCursor(in_key == PAGE_UP ? ARROW_UP : ARROW_DOWN);
    }
  } break;

  case ARROW_UP:
  case ARROW_DOWN:
  case ARROW_LEFT:
  case ARROW_RIGHT:
    editorMoveCursor(in_key);
    break;

  case CTRL_KEY('l'):
  case '\x1b':
    break;

  default:
    // insert any other keys into the text that aren't specially mapped by the
    // editor
    editorInsertChar(in_key);
    break;
  }

  quit_times = MILLI_QUIT_TIMES;
}

/***                                OUTPUT                                 ***/

// Scrolls the cursor down or up for large files
void editorScroll(void)
{
  edt_conf.render_x = 0x0;
  if (edt_conf.csr_y < edt_conf.num_rows) {
    edt_conf.render_x = editorRowCxToRx(edt_conf.row + edt_conf.csr_y, edt_conf.csr_x);
  }

  // handling vertical scrolling
  if (edt_conf.csr_y < edt_conf.row_off) {
    edt_conf.row_off = edt_conf.csr_y;
  }

  if (edt_conf.csr_y >= (edt_conf.row_off + edt_conf.term_rows)) {
    edt_conf.row_off = edt_conf.csr_y - edt_conf.term_rows + 1;
  }

  // handling horizontal scrolling
  if (edt_conf.render_x < edt_conf.col_off) {
    edt_conf.col_off = edt_conf.render_x;
  }

  if (edt_conf.render_x >= edt_conf.col_off + edt_conf.term_cols) {
    edt_conf.col_off = edt_conf.render_x - edt_conf.term_cols + 1;
  }
}

// Decorates the terminal interface with the content of the output screen buffer
void editorDrawRows(struct abuf* ab)
{
  int32_t y = 0;
  for (; y < edt_conf.term_rows - 1; ++y) {
    int32_t file_row = y + edt_conf.row_off;

    if (file_row >= edt_conf.num_rows) {
      // display welcome message only when an empty file is opened
      if (edt_conf.num_rows == 0 && y == edt_conf.term_rows / 3) {
        char welcome[80] = { '\0' };
        int32_t welcome_len = snprintf(welcome, sizeof(welcome), "Milli editor -- version %s",
            MILLI_VERSION);

        if (welcome_len > edt_conf.term_cols) {
          welcome_len = edt_conf.term_cols;
        }

        // center welcome message
        int32_t padding = (edt_conf.term_cols - welcome_len) / 2;
        if (padding) {
          abAppend(ab, "~", 1);
          --padding;
        }

        while (padding--) {
          abAppend(ab, " ", 1);
        }

        // write welcome message
        abAppend(ab, welcome, welcome_len);
      } else {
        abAppend(ab, "~", 1);
      }
    } else {
      // display contents of file
      int32_t len = edt_conf.row[file_row].rsize - edt_conf.col_off;

      if (len < 0) {
        len = 0x0;
      }

      if (len > edt_conf.term_cols) {
        len = edt_conf.term_cols;
      }

      CHAR_PTR ch = edt_conf.row[file_row].render + edt_conf.col_off;
      BYTE* hl = edt_conf.row[file_row].highlight + edt_conf.col_off;
      int32_t curr_color = -1;
      int32_t j = 0;
      for (; j < len; ++j) {
        if (iscntrl(ch[j])) {
          // highlighting non-printable characters
          char sym = (ch[j] <= 26) ? '@' + ch[j] : '?';

          abAppend(ab, "\x1b[7m", 4);
          abAppend(ab, &sym, 1);
          abAppend(ab, "\x1b[m", 3);

          if (curr_color != -1) {
            char buf[16] = { '\0' };
            int32_t clen = snprintf(buf, sizeof(buf), "\x1b[%dm", curr_color);
            abAppend(ab, buf, clen);
          }
        } else if (hl[j] == HL_NORMAL) {
          // highlight all other characters with white
          if (curr_color != -1) {
            abAppend(ab, "\x1b[39m", 5);
            curr_color = -1;
          }

          abAppend(ab, ch + j, 1);
        } else {
          // highlight all digits with RED
          int32_t color = editorSyntaxToColor(hl[j]);
          if (color != curr_color) {
            curr_color = color;
            char buf[16] = { '\0' };
            int32_t clen = snprintf(buf, sizeof(buf), "\x1b[%dm", color);
            abAppend(ab, buf, clen);
          }

          abAppend(ab, ch + j, 1);
        }
      }

      abAppend(ab, "\x1b[39m", 5);
    }

    // clear to the right of the cursor for each redrawn line
    abAppend(ab, "\x1b[K", 3);

    abAppend(ab, "\r\n", 2);
  }

  // drawing the last line
  abAppend(ab, "\x1b[K", 3);
}

void editorDrawStatusBar(struct abuf* ab)
{
  // display status bar with inverted colors: black text on a white background
  abAppend(ab, "\x1b[1;7m", 6);
  char status[80] = { '\0' };
  char rstatus[80] = { '\0' };
  // int32_t coverage_percent = ((edt_conf.csr_y + 1) / edt_conf.num_rows) *
  // 100;

  int32_t len = snprintf(status, sizeof(status), "%.20s - %d lines %s",
      edt_conf.fname ? edt_conf.fname : "[ No name ]",
      edt_conf.num_rows, edt_conf.dirty ? "[ modified ]" : "");
  int32_t rlen = snprintf(rstatus, sizeof(rstatus), "[ %s | Ln: %d, Col: %d ]",
      edt_conf.syntax ? edt_conf.syntax->file_type : "text",
      edt_conf.csr_y + 1, edt_conf.csr_x);

  if (len > edt_conf.term_cols) {
    len = edt_conf.term_cols;
  }

  abAppend(ab, status, len);

  while (len < edt_conf.term_cols) {
    if ((edt_conf.term_cols - len) == rlen) {
      abAppend(ab, rstatus, rlen);
      break;
    } else {
      abAppend(ab, " ", 1);
      ++len;
    }
  }

  abAppend(ab, "\x1b[m", 3);
  abAppend(ab, "\r\n", 2); // status message line
}

void editorDrawMsgBar(struct abuf* ab)
{
  abAppend(ab, "\x1b[K", 3);

  int32_t msg_len = strlen(edt_conf.status_msg);
  if (msg_len > edt_conf.term_cols) {
    msg_len = edt_conf.term_cols;
  }

  if (msg_len && (time(NULL) - edt_conf.status_msg_time) < STATUS_MSG_TIMEOUT) {
    abAppend(ab, edt_conf.status_msg, msg_len);
  }
}

// Refreshes and prepares the screen for decoration
void editorRefreshScreen(void)
{
  editorScroll();

  struct abuf ab = ABUF_INIT;

  // hide cursor
  abAppend(&ab, "\x1b[?25l", 6);

  // reposition the cursor to the top-left corner( use H command )
  abAppend(&ab, "\x1b[H", 3);

  editorDrawRows(&ab);
  editorDrawStatusBar(&ab);
  editorDrawMsgBar(&ab);

  // move the cursor
  char buf[32] = { '\0' };
  snprintf(buf, sizeof(buf), "\x1b[%d;%dH",
      1 + (edt_conf.csr_y - edt_conf.row_off),
      1 + (edt_conf.render_x - edt_conf.col_off));
  abAppend(&ab, buf, strlen(buf));

  // show cursor
  abAppend(&ab, "\x1b[?25h", 6);

  write(STDOUT_FILENO, ab.buffer, ab.len);
  abFree(&ab);
}

void editorSetStatusMessage(CONST_CHAR_PTR fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(edt_conf.status_msg, sizeof(edt_conf.status_msg), fmt, ap);
  va_end(ap);

  edt_conf.status_msg_time = time(NULL);
}

/***                                INIT                                   ***/

// Initializes the editor and its configuration
void initEditor(void)
{
  edt_conf.csr_x = edt_conf.csr_y = edt_conf.row_off = edt_conf.col_off = edt_conf.num_rows = edt_conf.dirty = edt_conf.render_x = 0x0;
  edt_conf.row = NULL;
  edt_conf.fname = NULL;
  INIT_ARRAY(edt_conf.status_msg, '\0');
  edt_conf.status_msg_time = 0;
  edt_conf.syntax = NULL;
  edt_conf.empty_file = 0;

  if (getTermWinSize(&edt_conf.term_rows, &edt_conf.term_cols) == -1) {
    HANDLE_ERR("getTermWinSize")
  }

  edt_conf.term_rows -= 2;
}

int32_t main(int32_t argc, CHAR_PTR argv[])
{
  enableRawMode();
  initEditor();

  if (argc >= 2) {
    edt_conf.fname = argv[1];
    editorOpen();
  }

  editorSetStatusMessage("HELP: Ctrl-S = save | Ctrl-F = find | Ctrl-Q = quit");

  for (;;) {
    editorRefreshScreen();
    editorProcessKeypress();
  }

  return EXIT_SUCCESS;
}
