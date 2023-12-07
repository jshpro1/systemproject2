#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 10

int clients[MAX_CLIENTS] = {0};

// 파일 수신 함수
void receive_file(int client_socket, const char *file_name) {
    char buffer[BUFFER_SIZE];
    int file_fd = open(file_name, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (file_fd == -1) {
        perror("파일 열기 실패");
        return;
    }

    ssize_t read_size;
    while ((read_size = read(client_socket, buffer, sizeof(buffer))) > 0) {
        if (write(file_fd, buffer, read_size) == -1) {
            perror("파일 쓰기 실패");
            close(file_fd);
            return;
        }
    }

    close(file_fd);
}

// 클라이언트 핸들링 함수에 파일 수신 호출 추가
void handle_client(int client_socket) {
    char buffer[BUFFER_SIZE];
    ssize_t read_size;

    while ((read_size = read(client_socket, buffer, sizeof(buffer))) > 0) {
        buffer[read_size] = '\0';

        // 클라이언트로부터 받은 메시지를 모든 클라이언트에게 방송
        printf("클라이언트: %s", buffer);

        // 파일 전송을 위한 프로토콜 추가
        if (strcmp(buffer, "FILE_TRANSFER_REQUEST") == 0) {
            // 클라이언트가 파일 전송을 요청했음을 알림
            read(client_socket, buffer, sizeof(buffer));
            printf("클라이언트가 파일을 전송합니다: %s\n", buffer);
            receive_file(client_socket, buffer);
            printf("파일 전송이 완료되었습니다.\n");
        }

        for (int i = 0; i < MAX_CLIENTS; ++i) {
            if (clients[i] != 0 && clients[i] != client_socket) {
                write(clients[i], buffer, read_size);
            }
        }
    }

    printf("클라이언트가 연결을 종료했습니다.\n");
    close(client_socket);
}

int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("소켓 생성 실패");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("바인딩 실패");
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, MAX_CLIENTS) == -1) {
        perror("대기열 설정 실패");
        exit(EXIT_FAILURE);
    }

    printf("채팅 서버가 클라이언트 연결을 대기 중입니다...\n");

    while (1) {
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_socket == -1) {
            perror("클라이언트 연결 수락 실패");
            exit(EXIT_FAILURE);
        }

        printf("클라이언트가 연결되었습니다.\n");

        for (int i = 0; i < MAX_CLIENTS; ++i) {
            if (clients[i] == 0) {
                clients[i] = client_socket;
                break;
            }
        }

        if (fork() == 0) {
            close(server_socket);
            handle_client(client_socket);
            close(client_socket);  // 파일 디스크립터 닫기 추가
            exit(EXIT_SUCCESS);
        } else {
            close(client_socket);
        }

        for (int i = 0; i < MAX_CLIENTS; ++i) {
            if (clients[i] == client_socket) {
                clients[i] = 0;
                break;
            }
        }
    }

    close(server_socket);

    return 0;
}

