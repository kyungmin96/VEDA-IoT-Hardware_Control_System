#include "buz_ctl.h"
#include <wiringPi.h>
#include <softTone.h>
#include <pthread.h>

#define BUZZER_PIN 25

static volatile int buzzer_status = 0;
static pthread_mutex_t buzzer_mutex = PTHREAD_MUTEX_INITIALIZER;

static int notes[] = {391, 391, 440, 440, 391, 391, 391, 391, 440, 440, 391, 391, 391, 391, 440, 440, 391, 391, 391, 391, 440, 440, 391, 391};

static void musicPlay() {
    softToneCreate(BUZZER_PIN);
    for (int i = 0; i < sizeof(notes)/sizeof(notes[0]); ++i) {
        pthread_mutex_lock(&buzzer_mutex);
        int stop_flag = (buzzer_status == 0);
        pthread_mutex_unlock(&buzzer_mutex);
        if (stop_flag) break;
        softToneWrite(BUZZER_PIN, notes[i]);
        delay(280);
    }
    softToneWrite(BUZZER_PIN, 0);
}

void set_status(int status) {
    pthread_mutex_lock(&buzzer_mutex);
    buzzer_status = status;
    pthread_mutex_unlock(&buzzer_mutex);
}

int get_status(void) {
    int status;
    pthread_mutex_lock(&buzzer_mutex);
    status = buzzer_status;
    pthread_mutex_unlock(&buzzer_mutex);
    return status;
}

void* thread(void* arg) {
    pinMode(BUZZER_PIN, OUTPUT);
    softToneCreate(BUZZER_PIN);
    while (1) {
        pthread_mutex_lock(&buzzer_mutex);
        int local_status = buzzer_status;
        pthread_mutex_unlock(&buzzer_mutex);
        if (local_status == 1) {
            musicPlay();
            set_status(0);
        }
        delay(100);
    }
    softToneWrite(BUZZER_PIN, 0);
    return NULL;
}

void set_callback(void(*cb)(void)) {
    // do nothing
}
