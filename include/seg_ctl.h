#pragma once
#include <pthread.h>

typedef void (*sevenseg_callback_t)(void);
void sevenseg_set_callback(sevenseg_callback_t cb);

void* sevenseg_thread(void* arg);
void sevenseg_set_num(int num);
int sevenseg_get_num(void);