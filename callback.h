#include "wifi-scan/wifi_scan.h"

const char itos[] = {"00010203040506070809"
                     "10111213141516171819"
                     "20212223242526272829"
                     "30313233343536373839"
                     "40414243444546474849"
                     "50515253545556575859"
                     "60616263646566676869"
                     "70717273747576777879"
                     "80818283848586878889"
                     "90919293949596979899"};

void *battery_init() {
  static char d[256];
  int pathlen = get_filepath("/sys/class/power_supply/", "BAT", d + 4);
  memcpy(d, &pathlen, 4);
  return d;
}

void battery(void *p, char *s) {
  static const char *const text[] = {
      "\xe2\x9a\xa1",     0, 0, "\xe2\x99\xbb\xef\xb8\x8f",
      "\xf0\x9f\x94\x8c", 0, 0, "\xf0\x9f\x94\x8b",
      "\xf0\x9f\x9b\x91"};
  static int lengths[] = {3, 0, 0, 6, 4, 0, 0, 4, 4};

  int fd;
  int *len, n, i;
  char *path;
  char buff[16];

  path = (char *)p + 4;
  len = p;

  strcpy(path + *len, "/status");
  fd = open(path, O_RDONLY);
  n = read(fd, buff, 16);
  close(fd);
  buff[n] = '\0';
  i = strchr(buff, '\n') - buff - 4;
  s += 1;
  memcpy(s, text[i], lengths[i]);

  strcpy(path + *len, "/capacity");
  fd = open(path, O_RDONLY);
  n = read(fd, buff, 16);
  close(fd);
  buff[n] = '\0';
  n = strchr(buff, '\n') - buff;
  s += lengths[i];
  s[0] = ' ';
  s += 1;
  memcpy(s, buff, n);
  s[n] = '%';
  s[n + 1] = ' ';
}

void datetime(void *p, char *s) {

  time_t tm;
  struct tm *timeinfo;
  time(&tm);
  timeinfo = localtime(&tm);
  s += 1;
  memcpy(s, itos + timeinfo->tm_hour * 2, 2);
  s += 2;
  s[0] = ':';
  s += 1;
  memcpy(s, itos + timeinfo->tm_min * 2, 2);
}

void *cpu_init() {
  int fd;
  fd = open("/proc/stat", O_RDONLY);

  return (void *)fd;
}

void cpu(void *p, char *s) {
  int fd = (int)p;

  static long ta[16], tb[16];
  static long ia[16], ib[16];
  static int idx, previdx = 0;
  long *total[2] = {ta, tb}, *idle[2] = {ia, ib};
  const uint32_t blk = 0xe29681;
  uint32_t cblk;

  char buff[96];
  char *base;
  int i, off, n, k;

  idx = previdx;
  read(fd, buff, 96);
  base = strchr(buff, '\n') + 1;
  off = base - buff;
  lseek(fd, off, SEEK_SET);

  k = get_nprocs();
  s += 1;
  while (k--) {
    n = read(fd, buff, 96);
    base = strchr(buff, ' ') + 1;
    total[idx][k] = 0;
    i = 4;
    while (i--) {
      idle[idx][k] = strtol(base, &base, 10);
      total[idx][k] += idle[idx][k];
    }
    previdx = (idx ^ 1) & 1;
    float idle_delta = idle[idx][k] - idle[previdx][k];
    float total_delta = total[idx][k] - total[previdx][k];

    float usage = 100.0 * (1.0 - idle_delta / total_delta);

    cblk = __bswap_32((blk + (int)(usage / 12.5)) << 8);
    memcpy(s, &cblk, 3);
    s += 3;

    base = strchr(buff, '\n') + 1;
    off = base - buff;
    lseek(fd, -(n - off), SEEK_CUR);
  }
  s += 1;
  lseek(fd, 0, SEEK_SET);
}

void cpu_click(void *p, char *s, int btn) {
  //  if (!fork())
  //    execl("/bin/xterm", "/bin/xterm", "-s", "htop", NULL);
}

void cpu_close(void *p) {
  int fd = (int)p;
  close(fd);
}

void temp(void *p, char *s) {
  static char icon[] = "\xf0\x9f\x8c\xa1";
  int n, i = 0, fd = (int)p;
  long vals[2];
  char buff[48];
  char *base;

  lseek(fd, 0, SEEK_SET);

  n = read(fd, buff, 48);
  buff[n] = '\0';

  base = strchr(buff, '\t');
  while (base) {
    if (i == 2)
      break;
    vals[i] = strtol(base, &base, 10);
    if (vals[i] > 0)
      i++;
  }

  memcpy(s, icon, 4);
  s += 4;
  if (vals[0] < 100)
    memcpy(s, itos + vals[0] * 2, 2);
  s += 3;
  if (vals[1] < 100)
    memcpy(s, itos + vals[1] * 2, 2);
}

void temp_close(void *p) {
  int fd = (int)p;
  close(fd);
}

void *temp_init() {
  int fd = open("/proc/acpi/ibm/thermal", O_RDONLY);
  return (void *)fd;
}
