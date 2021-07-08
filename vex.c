/*
 * vex.c: a terminal-based hex editor with vi-like keybinds
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <termios.h>
#include <sys/ioctl.h>

#define WIDTH  (ws.ws_col)
#define HEIGHT (ws.ws_row)

#define BUF_WIDTH  ((WIDTH-7)/4)
#define BUF_HEIGHT HEIGHT

#define MAXREAD (BUF_WIDTH*BUF_HEIGHT < filesize-(viewport*BUF_WIDTH) ? BUF_WIDTH*BUF_HEIGHT : filesize-(viewport*BUF_WIDTH))

#define REMOVE_TERMINATORS(c) (c >= ' ' && c <= '~' ? c : ' ')

enum {
  MODE_NORMAL = 128,
  MODE_INSERT = 255
};

enum {
  REDRAW_NONE,
  REDRAW_CHAR,
  REDRAW_LINE,
  REDRAW_FULL
};

FILE *fp;
unsigned char *filebuf,
              *viewbuf;
char *filename;
struct winsize ws;
struct termios tio;
int cursor_y = 1,
    cursor_x = 1,
    cursor_i = 0,
    viewport = 0,
    mode = MODE_NORMAL,
    changed = 0,
    written = 0,
    interactive = 0,
    bufsize,
    filesize;

static void vex_init();
static void vex_line();
static void vex_draw();
static void vex_save();
static void vex_comm();
static void vex_loop();
static void vex_stop();

#define vex_undraw_cursor() printf("\x1b[%i;1H\x1b[0;38;2;128;128;128m%.4x\x1b[%iG\x1b[0;38;2;128;128;128m%c", cursor_y, (cursor_y-1+viewport)*BUF_WIDTH, (BUF_WIDTH*3)+7+cursor_x, REMOVE_TERMINATORS(viewbuf[cursor_i]));

#define vex_draw_cursor() printf("\x1b[%i;1H\x1b[0;38;2;255;255;255m%.4x\x1b[%iG\x1b[0;38;2;0;0;0;48;2;%i;%i;%im%c\x1b[%i;%iH", cursor_y, cursor_i+(viewport*BUF_WIDTH), (BUF_WIDTH*3)+7+cursor_x, mode, mode, mode, REMOVE_TERMINATORS(viewbuf[cursor_i]), cursor_y, (cursor_x*3)+3); fflush(stdout);

#define vex_draw_char(line, col, i) printf("\x1b[%iG\x1b[0;38;2;%i;%i;%im%.2x \x1b[%iG\x1b[0;38;2;128;128;128m%c", (col*3)+6, (255-viewbuf[i]), (viewbuf[i]/2)+64, viewbuf[i], viewbuf[i], (BUF_WIDTH*3)+8+col, REMOVE_TERMINATORS(viewbuf[i]));

#define vex_draw_statusbar() if(mode==MODE_NORMAL){printf("\x1b[%i;1H\x1b[0m\x1b[2K\x1b[0;38;2;200;200;200m\"%s\" \x1b[0;38;2;166;162;140m(%ib)\x1b[0m\x1b[%i;%iH\x1b[2 q", HEIGHT, filename, filesize, cursor_x, cursor_y);}else{printf("\x1b[%i;1H\x1b[0m\x1b[2K\x1b[0;38;2;174;95;13m-- INSERT --\x1b[0m\x1b[%i;%iH\x1b[6 q", HEIGHT, cursor_x, cursor_y);}

void vex_init(int argc, char **argv){
  int line;
  struct termios raw;

  if(argc < 2 || argv[1] == NULL){
    printf("Usage: vex [file]\n");
    exit(1);
  } 

  fp = fopen(argv[1], "rb+");
  if(fp == NULL){
    printf("Error: Failed to open file \"%s\" for reading.\n", argv[1]);
    exit(2);
  }
  fseek(fp, 0, SEEK_END);
  filebuf = malloc((filesize=ftell(fp)));
  fseek(fp, 0, SEEK_SET);
  fread(filebuf, filesize, 1, fp);

  ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);

  filename = argv[1];
  viewbuf = filebuf;
 
  tcgetattr(STDIN_FILENO, &tio);
  raw = tio;
  raw.c_lflag &= ~(ECHO | ICANON);
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void vex_line(int line){
  int i;

  printf("\x1b[%i;1H\x1b[0;38;2;128;128;128m%.4x\x1b[%iG\x1b[0m|", line+1-viewport, line*BUF_WIDTH, (BUF_WIDTH*3)+6);
  for(i=line*BUF_WIDTH;i<(line*BUF_WIDTH)+BUF_WIDTH;i++){
    if(i-(viewport*BUF_WIDTH) < bufsize){
      vex_draw_char(line, i%BUF_WIDTH, i-(viewport*BUF_WIDTH));
    }
  }
}

void vex_draw(){
  int line;

  printf("\x1b[2J\x1b[?25l\x1b[?1002h\x1b[0;0H");
  for(line=viewport;line<BUF_HEIGHT+viewport;line++){
    vex_line(line);
  }
  printf("\x1b[2K\x1b[%i;%iH\x1b[?25h", cursor_y, cursor_x*3);
  
  vex_draw_statusbar();
}

void vex_save(){
  int writesize = BUF_WIDTH*BUF_HEIGHT;
  if(bufsize < writesize) writesize = bufsize;

  fseek(fp, viewport*BUF_WIDTH, SEEK_SET);
  fwrite(viewbuf, 1, writesize, fp);

  changed = 0;
  written = 1;
}

void vex_comm(){
  char cmd[256], buf[64];
  int nread, cur = 0;

  memset(cmd, '\0', sizeof(cmd));

  printf("\x1b[0m\x1b[%i;1H\x1b[2K:", BUF_HEIGHT);
  fflush(stdout);
  while((nread=read(STDIN_FILENO, buf, 64)) > 0){
    if(buf[0] == '\n'){
      printf("\x1b[2K");
      fflush(stdout);
      if((cmd[0] >= '0' && cmd[0] <= '9') ||
         (cmd[0] >= 'a' && cmd[0] <= 'f') ||
         (cmd[0] >= 'A' && cmd[0] <= 'F')){
        vex_undraw_cursor();
        cursor_i = strtol(cmd, NULL, 16);
        cursor_x = ((cursor_i%BUF_WIDTH)*3)+1;
        cursor_y = (cursor_i/BUF_WIDTH)+1;
        return;
      }
      switch(cmd[0]){
        case 'q':
          if(cmd[1] != '!' &&
             (changed && !written)){
            printf("\x1b[0G\x1b[91mNo write since last change (use :w to save, and :q! to force)");
            fflush(stdout);
            return;
          } else {
            vex_stop();
          }
          break;
        case 'w':
          vex_save();
          if(cmd[1] == 'q'){
            vex_stop();
          }
          return;
        default:
          printf("\x1b[0G\x1b[91mNot an editor command: %s", cmd);
          fflush(stdout);
          return;
      }
    } else if(buf[0] == '\x1b'){
      if(nread == 1){
        printf("\x1b[2K");
        fflush(stdout);
        return;
      } else switch(buf[2]){
        case 'A': /* Up arrow */
          /* TODO: History */
          break;
        case 'B': /* Down arrow */
          /* TODO: History */
          break;
        case 'C': /* Right arrow */
          if(cur < strlen(cmd)){
            cur++;
            printf("\x1b[%iG", cur+2);
            fflush(stdout);
          }
          break;
        case 'D': /* Left arrow */
         if(cur > 0){
            cur--;
            printf("\x1b[%iG", cur+2);
            fflush(stdout);
          }
          break;
      }
    } else if(buf[0] == 127 && cur > 0){
      cur--;
      memmove(cmd+cur, cmd+cur+1, strlen(cmd)-cur);
      printf("\x1b[0G\x1b[2K:%s\x1b[%iG", cmd, cur+2);
      fflush(stdout);
    } else {
      buf[nread] = '\0';
      memmove(cmd+cur+nread, cmd+cur, strlen(cmd)-cur);
      strncpy(cmd+cur, buf, nread);
      printf("\x1b[0G\x1b[2K:%s\x1b[%iG", cmd, cur+3);
      cur++;
      fflush(stdout);
    }
  }
}

void vex_loop(){
  int redraw = REDRAW_FULL,
      nibble = 0;
  char c[256];
  size_t n;
  bufsize = MAXREAD;

  goto skip_meaningful_check;

  do {
    if(c[0] == '\x1b'){
      redraw = REDRAW_NONE;
      nibble = 0;

      if(n == 1 || c[1] != '['){
        mode = MODE_NORMAL;
        vex_draw_statusbar();
        vex_undraw_cursor();
        goto skip_meaningful_check;
      }

      vex_undraw_cursor();
      switch(c[2]){
        case 'A': /* Up arrow */
          if(cursor_y > 1){
            cursor_y--;
            cursor_i -= BUF_WIDTH;
          } else if(viewport > 0){
            viewport--;
            redraw = REDRAW_FULL;
            viewbuf = &filebuf[viewport*BUF_WIDTH];
          }
          break;
        case 'B': /* Down arrow */
          if(cursor_i+BUF_WIDTH >= bufsize) break;
          if(cursor_y+1 < BUF_HEIGHT){
            cursor_y++;
            cursor_i += BUF_WIDTH;
          } else if(viewport*BUF_WIDTH <= filesize){
            viewport++;
            redraw = REDRAW_FULL;
            viewbuf = &filebuf[viewport*BUF_WIDTH];
          }
          break;
        case 'C': /* Right arrow */
          vex_undraw_cursor();
          redraw = REDRAW_NONE;
          if(cursor_x < BUF_WIDTH && cursor_i < bufsize) { cursor_x++; cursor_i++; }
          break;
        case 'D': /* Left arrow  */
          vex_undraw_cursor();
          redraw = REDRAW_NONE;
          if(cursor_x > 1 && cursor_i > 0) { cursor_x--; cursor_i--; }
          break;
        
        case '6': /* Page down */
          if(viewport*BUF_WIDTH < filesize){
            redraw = REDRAW_FULL;
            viewport += BUF_HEIGHT/2;
            viewbuf = &filebuf[viewport*BUF_WIDTH];
          }
          break;
        case '5': /* Page up   */
          if(viewport >= BUF_HEIGHT/2){
            redraw = REDRAW_FULL;
            viewport -= BUF_HEIGHT/2;
          }
          viewbuf = &filebuf[viewport*BUF_WIDTH];
          break;
        
        case 'M': /* Mouse */
          if(c[3] == ' '){        /* Mouse down */
            cursor_x = ((c[4]&0xff) - 0x21) / 3;
            cursor_y = ((c[5]&0xff) - 0x21) + 1;
            cursor_i = (cursor_y*BUF_WIDTH)+cursor_x;
          } else if(c[3] == 'a'){ /* Scroll down */
            redraw = REDRAW_FULL;
            viewport += 5;
            viewbuf = &filebuf[viewport*BUF_WIDTH];
          } else if(c[3] == '`'){ /* Scroll up */
            if(viewport >= 5){
              redraw = REDRAW_FULL;
              viewport -= 5;
              viewbuf = &filebuf[viewport*BUF_WIDTH];
            }
          }
          break;
      }
    } else
      if((c[0] >= '0' && c[0] <= '9') ||
         (c[0] >= 'a' && c[0] <= 'f') ||
         (c[0] >= 'A' && c[0] <= 'F') ){
      if(mode == MODE_INSERT){
        redraw = REDRAW_CHAR;
        c[1] = '\0';
        if(nibble == 0){
          viewbuf[cursor_i] = strtol(c, NULL, 16);
          if(cursor_i >= bufsize){
            if(bufsize == filesize){
              filesize++;
            }
            bufsize++;
          }
        } else {
          viewbuf[cursor_i] <<= 4;
          viewbuf[cursor_i] |= strtol(c, NULL, 16);
        }
        vex_draw_cursor();
        nibble++;
        if(nibble > 1){
          redraw = REDRAW_NONE;
          nibble = 0;
          vex_draw_char(cursor_y, cursor_i%BUF_WIDTH, cursor_i);
          cursor_i++;
          cursor_x++;
        }
        changed = 1;
      }
    } else switch(c[0]){
      case 'i':
        mode = MODE_INSERT;
        redraw = REDRAW_NONE;
        vex_draw_statusbar();
        break;
      case ':':
        redraw = REDRAW_NONE;
        if(mode == MODE_NORMAL){
          vex_comm();
        }
        break;
      case 'h': /* TODO: Remove redundancy with above arrow key handling */
        vex_undraw_cursor();
        redraw = REDRAW_NONE;
        if(cursor_x > 1) { cursor_x--; cursor_i--; }
        break;
      case 'j':
        vex_undraw_cursor();
        redraw = REDRAW_NONE;
        if(cursor_y+1 < BUF_HEIGHT){
          cursor_y++;
          cursor_i += BUF_WIDTH;
        } else { /* TODO */
          viewport++;
          redraw = REDRAW_FULL;
          viewbuf = &filebuf[viewport*BUF_WIDTH];
        }
        break;
      case 'k':
        vex_undraw_cursor();
        redraw = REDRAW_NONE;
        if(cursor_y > 1){
          cursor_y--;
          cursor_i -= BUF_WIDTH;
        } else if(viewport > 0){
          viewport--;
          redraw = REDRAW_FULL;
          viewbuf = &filebuf[viewport*BUF_WIDTH];
        }
        break;
      case 'l':
        vex_undraw_cursor();
        redraw = REDRAW_NONE;
        if(cursor_x < BUF_WIDTH) { cursor_x++; cursor_i++; }
        break;
      default:
        redraw = REDRAW_NONE;
        continue;
    }

skip_meaningful_check:;
    switch(redraw){
      case REDRAW_FULL:
        vex_draw();
        break;
      case REDRAW_LINE:
        vex_line(cursor_y);
        break;
      case REDRAW_CHAR:
        vex_draw_char(cursor_y, cursor_i%BUF_WIDTH, cursor_i);
        break;
    }
    vex_draw_cursor();
  } while((n=read(STDIN_FILENO, c, 255)) > 0);
}

void vex_stop(){
  fclose(fp);
  free(filebuf);
 
  printf("\x1b[?1002l\x1b[0m\x1b[0;0H\x1b[2J");
  fflush(stdout);
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &tio);

  exit(0);
}

int main(int argc, char **argv){
  vex_init(argc, argv);
  vex_loop();
  vex_stop();

  return 0;
}
