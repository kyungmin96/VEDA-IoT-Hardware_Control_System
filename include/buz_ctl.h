#pragma once
#include <pthread.h>

void* buzzer_thread(void* arg);
void buzzer_set_status(int status);
int buzzer_get_status(void);