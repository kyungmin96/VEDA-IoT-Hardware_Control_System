#include "seg_ctl.h"
#include <wiringPi.h>
#include <pthread.h>
#include <time.h>

static volatile int seven_seg_num = -1;
static pthread_mutex_t seven_mutex = PTHREAD_MUTEX_INITIALIZER;
static sevenseg_callback_t user_callback = NULL; // 콜백 타입 명시적 선언

static int gpiopins[4] = {23, 18, 15, 14};
static int seven_number[10][4] = {
    {0,0,0,0}, {0,0,0,1}, {0,0,1,0}, {0,0,1,1}, {0,1,0,0},
    {0,1,0,1}, {0,1,1,0}, {0,1,1,1}, {1,0,0,0}, {1,0,0,1}
};

void set_status(int num) {
    pthread_mutex_lock(&seven_mutex);
    seven_seg_num = num;
    pthread_mutex_unlock(&seven_mutex);
}

int get_status(void) {
    int num;
    pthread_mutex_lock(&seven_mutex);
    num = seven_seg_num;
    pthread_mutex_unlock(&seven_mutex);
    return num;
}

/* 1. 콜백 타입 명확히 정의 */
void set_callback(sevenseg_callback_t cb) {
    user_callback = cb;
}

static void setup_7seg_pins() {
    for (int i = 0; i < 4; i++) {
        pinMode(gpiopins[i], OUTPUT);
        digitalWrite(gpiopins[i], HIGH);
    }
}

static void display_7seg(int num) {
    if (num < 0 || num > 9) {
        for (int i = 0; i < 4; i++) digitalWrite(gpiopins[i], HIGH);
        return;
    }
    for (int i = 0; i < 4; i++)
        digitalWrite(gpiopins[i], seven_number[num][i] ? HIGH : LOW);
}

void* thread(void* arg) {
    setup_7seg_pins();
    time_t last_decrement = 0;
    int current_num = -1;
    while (1) {
        pthread_mutex_lock(&seven_mutex);
        int num = seven_seg_num;
        pthread_mutex_unlock(&seven_mutex);
        
        if (num != current_num) {
            current_num = num;
            last_decrement = time(NULL);
        }
        
        if (current_num >= 0) {
            display_7seg(current_num);
            delay(50);
            time_t now = time(NULL);
            if (now - last_decrement >= 1) {
                if (current_num > 0) {
                    current_num--;
                    set_status(current_num);
                    last_decrement = now;
                } else {
                    /* 2. 콜백 실행 전 NULL 체크 강화 */
                    if (user_callback != NULL) {
                        user_callback(); // 인자 없이 호출
                    }
                    set_status(-1);
                    current_num = -1;
                }
            }
        } else {
            display_7seg(-1);
        }
        delay(100);
    }
    return NULL;
}
