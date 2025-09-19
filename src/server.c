#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>
#include <wiringPi.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <syslog.h>

#define PORT 60000
#define MAX_DEVICES 4
#define LIB_PREFIX "lib"
#define LIB_SUFFIX ".so"
#define LIGHT_THRESHOLD 180

typedef struct {
    void* handle;
    void (*set_status)(int);
    int (*get_status)(void);
    void* (*thread_func)(void*);
    void (*set_callback)(void(*)(void));
} DeviceInterface;

volatile sig_atomic_t server_running = 1;
int sockfd;

// 장치별 라이브러리 정보
const char* device_libs[MAX_DEVICES] = {
    "led_ctl", "buz_ctl", "light_ctl", "seg_ctl"
};
DeviceInterface devices[MAX_DEVICES];
pthread_t device_threads[MAX_DEVICES];
pthread_t monitor_tid; 

/* 데몬 초기화 함수 */
void daemonize() {
    pid_t pid = fork();
    if (pid < 0) {
        syslog(LOG_ERR, "fork 실패: %m");
        exit(EXIT_FAILURE);
    }
    if (pid > 0) exit(EXIT_SUCCESS);

    if (setsid() < 0) {
        syslog(LOG_ERR, "setsid 실패: %m");
        exit(EXIT_FAILURE);
    }

    umask(0);
    if (chdir("/") < 0) {
        syslog(LOG_ERR, "chdir 실패: %m");
    }

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
}

/* 신호 핸들러 */
void handle_signal(int sig) {
    syslog(LOG_INFO, "서버 종료 신호(%d) 수신", sig);
    server_running = 0;
}

/* 동적 라이브러리 로드 */
int load_device(const char* base_name, int index) {
    char lib_path[256];
    snprintf(lib_path, sizeof(lib_path), "%s%s%s", 
             LIB_PREFIX, base_name, LIB_SUFFIX);

    void* handle = dlopen(lib_path, RTLD_LAZY);
    if (!handle) {
        syslog(LOG_ERR, "%s 로드 실패: %s", lib_path, dlerror());
        return -1;
    }

    devices[index].handle = handle;
    devices[index].set_status = dlsym(handle, "set_status");
    devices[index].get_status = dlsym(handle, "get_status");
    devices[index].thread_func = dlsym(handle, "thread");
    devices[index].set_callback = dlsym(handle, "set_callback");

    if (!devices[index].thread_func) {
        syslog(LOG_ERR, "%s 심볼 누락", base_name);
        dlclose(handle);
        return -1;
    }

    syslog(LOG_INFO, "%s 초기화 완료", base_name);
    return 0;
}

void buzzer_on_callback(void) {
    if (devices[1].set_status)
        devices[1].set_status(1);
}

/* 장치 간 콜백 설정 */
void setup_inter_device_callbacks() {
    // 7세그먼트 완료 시 부저 울리기
    if (devices[3].set_callback) {
        devices[3].set_callback(buzzer_on_callback);
    }
}

/* 장치 스레드 시작 */
void start_device_threads() {
    const char* thread_names[MAX_DEVICES] = {
        "LED", "Buzzer", "Light", "7Segment"
    };

    for (int i = 0; i < MAX_DEVICES; i++) {
        if (pthread_create(&device_threads[i], NULL, 
                          devices[i].thread_func, NULL) != 0) {
            syslog(LOG_ERR, "%s 스레드 시작 실패", thread_names[i]);
            exit(EXIT_FAILURE);
        }
    }
}

void* light_monitor_thread(void* arg) {
    while(server_running) {
        int current_light = devices[2].get_status();  // light_ctl에서 조도 값 읽기
        
        // 조도가 임계값보다 높을 때 (어두운 환경) LED 켜기
        if(current_light > LIGHT_THRESHOLD) {
            devices[0].set_status(100);  // LED ON
            syslog(LOG_INFO, "LED ON (조도: %d)", current_light);
        } else {
            devices[0].set_status(0);    // LED OFF
            syslog(LOG_INFO, "LED OFF (조도: %d)", current_light);
        }
        sleep(1);
    }
    return NULL;
}

/* 클라이언트 핸들러 스레드 */
void* client_handler(void* arg) {
    int csock = *((int*)arg);
    free(arg);

    while(server_running) {
        int code;
        if (recv(csock, &code, sizeof(code), 0) <= 0) break;

        syslog(LOG_DEBUG, "수신 코드: %d", code);
        
        switch(code) {
            case 0: devices[0].set_status(100); break;
            case 1: devices[0].set_status(50); break;
            case 2: devices[0].set_status(10); break;
            case 3: devices[0].set_status(0); break;
            case 4: devices[1].set_status(1); break;
            case 5: devices[1].set_status(0); break;
            case 6: {
                int val = devices[2].get_status();
                send(csock, &val, sizeof(val), 0);
                break;
            }
            case -2: goto EXIT;
            default:
                if(code >= 7 && code <= 97) {
                    devices[3].set_status((code - 7) / 10);
                }
        }
    }

EXIT:
    close(csock);
    return NULL;
}

/* 메인 함수 */
int main() {
    openlog("server", LOG_PID, LOG_DAEMON);
    daemonize();

    struct sigaction sa = {
        .sa_handler = handle_signal,
        .sa_flags = SA_RESTART
    };
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    if (wiringPiSetupGpio() == -1) {
        syslog(LOG_ERR, "wiringPi 초기화 실패");
        goto cleanup;
    }

    // 동적 라이브러리 로드
    for (int i = 0; i < MAX_DEVICES; i++) {
        if (load_device(device_libs[i], i) < 0) {
            syslog(LOG_ERR, "장치 초기화 실패");
            goto cleanup;
        }
    }

    setup_inter_device_callbacks();
    start_device_threads();

    if (pthread_create(&monitor_tid, NULL, light_monitor_thread, NULL) != 0) {
        syslog(LOG_ERR, "조도 모니터링 스레드 생성 실패");
        goto cleanup;
    }

    // 네트워크 설정
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv_addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(PORT)
    };

    if (bind(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        syslog(LOG_ERR, "바인드 실패: %m");
        goto cleanup;
    }

    if (listen(sockfd, 5) < 0) {
        syslog(LOG_ERR, "리슨 실패: %m");
        goto cleanup;
    }

    syslog(LOG_INFO, "서버 시작 (포트: %d)", PORT);

    while (server_running) {
        struct sockaddr_in cli_addr;
        socklen_t addr_len = sizeof(cli_addr);
        
        int csock = accept(sockfd, (struct sockaddr*)&cli_addr, &addr_len);
        if (csock < 0) {
            if (errno == EINTR) continue;
            syslog(LOG_ERR, "accept 오류: %m");
            continue;
        }

        int *client_sock = malloc(sizeof(int));
        *client_sock = csock;
        pthread_t tid;
        if (pthread_create(&tid, NULL, client_handler, client_sock) != 0) {
            syslog(LOG_ERR, "클라이언트 핸들러 생성 실패");
            free(client_sock);
            close(csock);
        }
        pthread_detach(tid);
    }

cleanup:
    if (sockfd >= 0) close(sockfd);
    
    // 스레드 정리
    for (int i = 0; i < MAX_DEVICES; i++) {
        if (devices[i].handle) {
            dlclose(devices[i].handle);
        }
    }

    pthread_join(monitor_tid, NULL);

    syslog(LOG_INFO, "서버 정상 종료");
    closelog();
    return 0;
}
