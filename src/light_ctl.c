#include "light_ctl.h"
#include <wiringPiI2C.h>
#include <pthread.h>

static volatile int light_level = 0;
static pthread_mutex_t light_mutex = PTHREAD_MUTEX_INITIALIZER;

void set_status(int status) {
    // do nothing
}

int get_status(void) {
    int level;
    pthread_mutex_lock(&light_mutex);
    level = light_level;
    pthread_mutex_unlock(&light_mutex);
    return level;
}

void* thread(void* arg) {
    int fd = wiringPiI2CSetupInterface("/dev/i2c-1", 0x48);
    int a2dChannel = 0;
    while (1) {
        wiringPiI2CWrite(fd, 0x00 | a2dChannel);
        wiringPiI2CRead(fd); // 더미
        int a2dVal = wiringPiI2CRead(fd);
        pthread_mutex_lock(&light_mutex);
        light_level = a2dVal;
        pthread_mutex_unlock(&light_mutex);
        sleep(1);
    }
    return NULL;
}

void set_callback(void(*cb)(void)) {
    // do nothing
}
