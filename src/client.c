#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>

#define MAXDATASIZE 100

int sockfd = -1;
int client_running = 1;

// 명령어 보기 출력 함수
void print_menu() {
    printf("\n------MENU------\n");
    printf("1. led_on_MAX   : LED를 최대 밝기로 켭니다.\n");
    printf("2. led_on_MID   : LED를 중간 밝기로 켭니다.\n");
    printf("3. led_on_MIN   : LED를 최소 밝기로 켭니다.\n");
    printf("4. led_off      : LED를 끕니다.\n");
    printf("5. music_on     : 음악을 재생합니다.\n");
    printf("6. music_off    : 음악을 중지합니다.\n");
    printf("7. light        : 현재 조도 센서 값을 확인합니다.\n");
    printf("8. 7seg         : 7세그먼트에 숫자를 표시합니다.\n");
    printf("9. exit         : 프로그램을 종료합니다.\n");
    printf("cmd: ");
}

void clear_input_buffer() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

int get_command_code(const char* input) {
    if ((strcmp(input, "led_on_MAX") == 0) || (strcmp(input, "1") == 0)) return 0;
    if ((strcmp(input, "led_on_MID") == 0) || (strcmp(input, "2") == 0)) return 1;
    if ((strcmp(input, "led_on_MIN") == 0) || (strcmp(input, "3") == 0)) return 2;
    if ((strcmp(input, "led_off") == 0) || (strcmp(input, "4") == 0)) return 3;
    if ((strcmp(input, "music_on") == 0) || (strcmp(input, "5") == 0)) return 4;
    if ((strcmp(input, "music_off") == 0) || (strcmp(input, "6") == 0)) return 5;
    if ((strcmp(input, "light") == 0) || (strcmp(input, "7") == 0)) return 6;
    if ((strcmp(input, "7seg") == 0) || (strcmp(input, "8") == 0)) {
        int number;
        printf("숫자를 입력하세요 (0-9) : ");
        if (scanf("%d", &number) != 1) {
            clear_input_buffer();
            printf("유효하지 않은 입력\n");
            return -999;
        }
        
        clear_input_buffer(); // 입력 버퍼 클리어
        
        if (number < 0 || number > 9) {
            printf("유효하지 않은 숫자 (0-9)\n");
            return -999;
        }
        
        return (7 + number * 10);
    }
    if ((strcmp(input, "exit") == 0) || (strcmp(input, "9") == 0)) return -2;
    return -999; // Invalid
}

// SIGINT 핸들러 (Ctrl+C)
void signal_handler(int sig) {
    printf("\n프로그램 종료 요청됨...\n");
    if (sockfd >= 0) {
        int code = -2; // EXIT 코드
        send(sockfd, &code, sizeof(code), 0);
        close(sockfd);
    }
    client_running = 0;
    exit(0);
}

int main(int argc, char *argv[]) {
    struct hostent *he;
    struct sockaddr_in server_addr;

    // 시그널 핸들러 설정
    signal(SIGINT, signal_handler);

    if(argc != 2) {
        fprintf(stderr, "사용법: %s 호스트명\n", argv[0]);
        exit(1);
    }

    if((he = gethostbyname(argv[1])) == NULL) {
        perror("gethostbyname");
        exit(1);
    }
    
    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(1);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(60000);
    server_addr.sin_addr = *((struct in_addr *)he->h_addr);
    memset(&(server_addr.sin_zero), '\0', 8);

    printf("서버에 연결 중...\n");
    if(connect(sockfd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) == -1) {
        perror("connect");
        close(sockfd);
        exit(1);
    }
    printf("서버에 연결되었습니다.\n");

    char input[30];
    int code;
    
    while (client_running) {
        print_menu();
        
        if (fgets(input, sizeof(input), stdin) == NULL) {
            break;
        }
        
        // 개행 문자 제거
        input[strcspn(input, "\n")] = 0;
        
        if (strlen(input) == 0) {
            continue;  // 빈 입력은 무시
        }
        
        code = get_command_code(input);
        if (code == -999) {
            printf("잘못된 명령어입니다. 다시 입력해주세요.\n");
            continue;
        }

        if(send(sockfd, &code, sizeof(code), 0) == -1) {
            perror("send");
            break;
        }
     
        printf("명령 코드 %d 전송 완료\n", code + 1);
        
        if (code == 6){
            int light_value;
            if (recv(sockfd, &light_value, sizeof(light_value), 0) < 0) {
                perror("수신 실패");
                break;
            }
            
            printf("서버에서 받은 조도 센서 값: %d\n", light_value);
        }

        if (code == -2) {
            printf("프로그램을 종료합니다.\n");
            break;
        }
        
        // 명령 전송 후 잠시 대기
        usleep(500000);  // 0.5초
    }

    close(sockfd);
    return 0;
}