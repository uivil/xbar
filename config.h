
#ifndef CONFIG_H
#define CONFIG_H

#include "callback.h"

#define NUMBLOCKS 6
const char *fontname = "monospace:size=10";
char *const delimiter = "\xe2\x96\x95";
char barheight = 20; // px

interval_func interval_clbk[] = {weather, alsa, temp, cpu, battery, datetime};
click_func click_clbk[] = {0, alsa_click, 0, cpu_click, 0, 0};
init_func init_clbk[] = {weather_init, alsa_init,    temp_init,
                         cpu_init,     battery_init, 0};
close_func close_clbk[] = {weather_close, alsa_close, temp_close,
                           cpu_close,     0,          0};
int interval[] = {60 * 60 * 2, 1, 1, 1, 2, 10};
void *user_ptr[NUMBLOCKS];
int block_size[] = {24, 8, 9, 25, 10, 7};

#endif
