#include "led_ctl.h"
#include <wiringPi.h>
#include <softPwm.h>
#include <pthread.h>

#define LED_PIN 21

static volatile int led_status = 0;
static pthread_mutex_t led_mutex = PTHREAD_MUTEX_INITIALIZER;

void set_status(int status) {
    pthread_mutex_lock(&led_mutex);
    led_status = status;
    pthread_mutex_unlock(&led_mutex);
}

int get_status(void) {
    int status;
    pthread_mutex_lock(&led_mutex);
    status = led_status;
    pthread_mutex_unlock(&led_mutex);
    return status;
}

void* thread(void* arg) {
    softPwmCreate(LED_PIN, 0, 100);
    while (1) {
        pthread_mutex_lock(&led_mutex);
        softPwmWrite(LED_PIN, led_status);
        pthread_mutex_unlock(&led_mutex);
        delay(100);
    }
    softPwmWrite(LED_PIN, 0);
    return NULL;
}

void set_callback(void(*cb)(void)) {
    // do nothing
}
