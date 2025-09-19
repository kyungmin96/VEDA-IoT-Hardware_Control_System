#pragma once
#include <pthread.h>

void* led_thread(void* arg);
void led_set_status(int status);
int led_get_status(void);