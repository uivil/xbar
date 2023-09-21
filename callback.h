#include "wifi-scan/wifi_scan.h"
#include <curl/curl.h>

#define MIN(a, b) (a) > (b) ? (b) : (a)
#define MAX(a, b) (a) > (b) ? (a) : (b)

const char itos[] = {"00010203040506070809"
                     "10111213141516171819"
                     "20212223242526272829"
                     "30313233343536373839"
                     "40414243444546474849"
                     "50515253545556575859"
                     "60616263646566676869"
                     "70717273747576777879"
                     "80818283848586878889"
                     "90919293949596979899100"};

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

struct alsa_cb {
  snd_mixer_selem_id_t *sid;
  snd_mixer_t *handle;
  snd_mixer_elem_t *elem;
  long volume, minvol, maxvol;
};

void alsa_cpy(char *s, long vol) {
  static char icon[] = {0xf0, 0x9f, 0x94, 0x88};
  memcpy(s, icon, 4);
  s += 5;
  s[2] = ' ';
  memcpy(s, itos + vol * 2, 2 + (vol > 99));
}

void alsa(void *p, char *s) {
  static char icon[] = {0xf0, 0x9f, 0x94, 0x88};
  struct alsa_cb *cb = p;
  long vol;

  snd_mixer_selem_get_playback_volume(cb->elem, 0, &cb->volume);
  vol = cb->volume * 100 / (cb->maxvol - cb->minvol);
  alsa_cpy(s, vol);
}

int alsa_click(void *p, char *s, int btn) {
  struct alsa_cb *cb = p;
  int inc[2] = {3, -3};
  long vol;

  cb->volume += inc[btn];
  cb->volume = MAX(cb->volume, cb->minvol);
  cb->volume = MIN(cb->volume, cb->maxvol);
  snd_mixer_selem_set_playback_volume_all(cb->elem, cb->volume);
  vol = cb->volume * 100 / (cb->maxvol - cb->minvol);
  alsa_cpy(s, vol);

  return 1;
}

void *alsa_init() {
  struct alsa_cb *cb = malloc(sizeof(*cb));
  const char *card = "default";
  const char *selem_name = "Master";

  snd_mixer_open(&cb->handle, 0);
  snd_mixer_attach(cb->handle, card);
  snd_mixer_selem_register(cb->handle, NULL, NULL);
  snd_mixer_load(cb->handle);

  snd_mixer_selem_id_malloc(&cb->sid);
  snd_mixer_selem_id_set_index(cb->sid, 0);
  snd_mixer_selem_id_set_name(cb->sid, selem_name);
  cb->elem = snd_mixer_find_selem(cb->handle, cb->sid);
  snd_mixer_selem_get_playback_volume_range(cb->elem, &cb->minvol, &cb->maxvol);
  return cb;
}

void alsa_close(void *p) {
  struct alsa_cb *cb = p;
  snd_mixer_selem_id_free(cb->sid);
  snd_mixer_close(cb->handle);
  free(cb);
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
  // padding 1 space
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

int cpu_click(void *p, char *s, int btn) {
  if (!fork())
    execl("/bin/xterm", "/bin/xterm", "-s", "btop", NULL);
  return 0;
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

struct weather_cb {
  CURL *curl;
};

size_t weather_writefunc(char *ptr, size_t size, size_t nmemb, void *userdata) {
  char *s = userdata;
  memcpy(s, ptr, nmemb);
  return nmemb * size;
}

void *weather_init() {
  static char *url = "https://wttr.in/?T&format=%c%f%20%w";
  CURLcode res;
  struct weather_cb *cb = malloc(sizeof(*cb));

  cb->curl = curl_easy_init();
  if (!cb->curl)
    return 0;
  curl_easy_setopt(cb->curl, CURLOPT_URL, url);
  curl_easy_setopt(cb->curl, CURLOPT_WRITEFUNCTION, weather_writefunc);
  curl_easy_setopt(cb->curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
  return cb;
}

void weather(void *p, char *s) {
  struct weather_cb *cb = p;
  curl_easy_setopt(cb->curl, CURLOPT_WRITEDATA, s);
  CURLcode res;
  res = curl_easy_perform(cb->curl);
}

void weather_close(void *p) {
  struct weather_cb *cb = p;
  curl_easy_cleanup(cb->curl);
  free(cb);
}

struct net_cb {
  char wifi_path[256];
  int wifi_len;
  char ether_path[256];
  int ether_len;
};
// : wifi disabled
// : no wifi connection
// : wifi connection with quality
// : no ethernet
// : ethernet working
// : vpn is active

void net(void *p, char *s) {

  static char *wifi_icons[] = {"\xf0\x9f\x93\xa1", "\xf0\x9f\x93\xb6",
                               "\xe2\x9d\x8c"};
  static char *ether_icons[] = {"\xe2\x9d\x8e", "\xf0\x9f\x8c\x90"};
  int fd, n, len;
  char c;
  char buff[64], *base;
  long link;
  struct net_cb cb = *(struct net_cb *)p;

  // WiFi
  if (cb.wifi_len > 0) {
    strcpy(cb.wifi_path + cb.wifi_len, "/carrier");
    fd = open(cb.wifi_path, O_RDONLY);
    n = read(fd, &c, 1);
    close(fd);
    c = n < 1 ? 2 : c - '0';
    len = 4 - (c == 2);
    memcpy(s, wifi_icons[c], len);
    s += len + 1;
    memset(s, ' ', 10);
    if (c == 1) {
      fd = open("/proc/net/wireless", O_RDONLY);
      lseek(fd, 163, SEEK_SET);
      n = read(fd, buff, 64);
      close(fd);
      buff[n] = '\0';
      char *base = strchr(buff, ':') + 1;
      n = 2;
      while (n--) {
        link = strtol(base, &base, 10);
      }
      link = link * 100 / 70;
      len = 2 + (link > 99);
      memcpy(s, itos + link * 2, len);
      s += len;
      *s = '%';
      s += 1;
    }
  }

  if (cb.ether_len > 0) {
    strcpy(cb.ether_path + cb.ether_len, "/carrier");
    fd = open(cb.ether_path, O_RDONLY);
    n = read(fd, &c, 1);
    close(fd);
    c = n < 1 ? 2 : c - '0';
    len = 4 - (c == 0);
    memcpy(s + 2, ether_icons[c], len);
  }
}

// https://unix.stackexchange.com/questions/193845/how-to-determine-interface-running-state-without-ifconfig

void *net_init() {
  static struct net_cb cb;
  int pathlen;
  cb.wifi_len = get_filepath("/sys/class/net/", "w", cb.wifi_path);
  cb.ether_len = get_filepath("/sys/class/net/", "e", cb.ether_path);
  return &cb;
}

void net_close(void *p) {}

int net_click(void *p, char *s, int btn) {
  if (!fork())
    execl("/bin/iwgtk", "/bin/iwgtk", NULL);
  return 0;
}
