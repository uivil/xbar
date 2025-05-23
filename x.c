
int get_filepath(char *path, char *keyword, char *dev);
#include "all.h"
#include "config.h"

struct {
  Display *d;
  int w, h;
} dpy;

int scr;
Window win;
XftFont *font;

struct block blocks[NUMBLOCKS];

int get_filepath(char *path, char *keyword, char *dev) {
  struct dirent *dir;
  DIR *d;
  int pathlen, fd, n;

  strcpy(dev, path);
  pathlen = strlen(path);
  d = opendir(path);
  if (!d)
    return -1;
  while ((dir = readdir(d)) != NULL) {
    if (strstr(dir->d_name, keyword) != NULL)
      break;
  }
  if (dir == NULL)
    return -1;
  fd = open(path, O_RDONLY);
  n = readlinkat(fd, dir->d_name, dev + pathlen, 256);
  close(fd);
  closedir(d);
  if (n == -1)
    return -1;
  n += pathlen;
  dev[n] = '\0';
  return n;
}

int string_to_glyphs_width(const char *string, int size) {
  XGlyphInfo e;
  XftTextExtentsUtf8(dpy.d, font, string, size, &e);
  return e.xOff;
}

int process(long timestamp) {
  int i = 0, off = 0, width = 0, dirty = 0;
  for (off = 0, i = 0; i < NUMBLOCKS; i++) {
    blocks[i].str = bar.str + off;
    if (timestamp % interval[i] == 0 && interval_clbk[i]) {
      interval_clbk[i](user_ptr[i], blocks[i].str);
      dirty = 1;
    }
    blocks[i].w = string_to_glyphs_width(blocks[i].str, block_size[i]);
    width += blocks[i].w;
    off += block_size[i];
    if (i == NUMBLOCKS - 1)
      break;
    memcpy(bar.str + off, delim.str, delim.size);
    off += delim.size;
  }
  bar.str[off] = '\0';
  bar.size = off;
  bar.w = width + delim.w * (NUMBLOCKS - 1);
  return dirty;
}

int running = 1;

void sighandler(int sig) { running = 0; }
int rr = 0;
void alr(int sig) {
  rr = 1;
  signal(SIGALRM, alr);
}

int main() {
  int timestamp = 0, fd, n, i, maxsize = 0;
  XEvent xev;
  struct input_event evt;
  struct timespec ts;
  int clock_type = CLOCK_MONOTONIC_RAW;
  clock_gettime(clock_type, &ts);
  long tic, toc, delta = 0, prevdelta;
  int oneshot = 1;

  signal(SIGINT, sighandler);

  dpy.d = XOpenDisplay(0);
  scr = XDefaultScreen(dpy.d);
  win = XDefaultRootWindow(dpy.d);
  dpy.w = DisplayWidth(dpy.d, scr);

  font = XftFontOpenName(dpy.d, scr, fontname);
  bar.w = DisplayWidth(dpy.d, scr);
  bar.h = barheight;

  delim.str = delimiter;
  delim.size = strlen(delim.str);
  delim.w = string_to_glyphs_width(delim.str, delim.size);

  i = NUMBLOCKS;
  while (i--) {
    if (init_clbk[i])
      user_ptr[i] = init_clbk[i]();
    maxsize += block_size[i];
  }
  bar.size = NUMBLOCKS * (maxsize + 1);
  bar.str = malloc(bar.size);
  memset(bar.str, ' ', bar.size);

  clock_gettime(clock_type, &ts);
  tic = ts.tv_sec;

  process(0);
  XStoreName(dpy.d, win, bar.str);
  XFlush(dpy.d);
  while (running) {
    usleep(100);
    prevdelta = delta;
    clock_gettime(clock_type, &ts);
    toc = ts.tv_sec;
    delta = toc - tic;
    if (delta != prevdelta) {
      if (oneshot == 1 && process(delta)) {
        XStoreName(dpy.d, win, bar.str);
        XFlush(dpy.d);
      }
      oneshot = 0;
    } else {
      oneshot = 1;
    }
  }

  i = NUMBLOCKS;
  while (i--) {
    if (close_clbk[i])
      close_clbk[i](user_ptr[i]);
  }

  XftFontClose(dpy.d, font);
  XFlush(dpy.d);
  XCloseDisplay(dpy.d);
}
