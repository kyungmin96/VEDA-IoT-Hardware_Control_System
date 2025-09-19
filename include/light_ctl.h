#pragma once
#include <pthread.h>

void* light_thread(void* arg);
int light_get_level(void);