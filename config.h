
#ifndef CONFIG_H
#define CONFIG_H

#include "callback.h"

#define NUMBLOCKS 4
const char *fontname = "monospace:size=10";
char *const delimiter = "\xe2\x96\x95";
char barheight = 20; // px

interval_func interval_clbk[] = { temp, cpu, battery, datetime};
click_func click_clbk[] = { 0, cpu_click, 0, 0};
init_func init_clbk[] = {temp_init, cpu_init, battery_init, 0};
close_func close_clbk[] = {temp_close, cpu_close, 0, 0};
int interval[] = {1, 1, 2, 10};
void *user_ptr[NUMBLOCKS];
int block_size[] = {9, 25, 10, 7};

#endif
