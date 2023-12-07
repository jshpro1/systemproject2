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

typedef struct {
    int socket;
    char username[50];
} Client;

Client clients[MAX_CLIENTS] = {{0, ""}};

// 모든 클라이언트에게 메시지 방송
void broadcast_message_to_all(const char *message) {
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i].socket != 0) {
            write(clients[i].socket, message, strlen(message));
        }
    }
}

// 클라이언트 처리 함수
void handle_client(Client client) {
    char buffer[BUFFER_SIZE];
    ssize_t read_size;

    // 사용자 등록
    read(client.socket, client.username, sizeof(client.username));
    printf("클라이언트 등록: %s\n", client.username);

    // 새로운 사용자가 입장했음을 알림
    char join_message[150];
    snprintf(join_message, sizeof(join_message), "%s 님이 입장하셨습니다.", client.username);
    broadcast_message_to_all(join_message);

    while ((read_size = read(client.socket, buffer, sizeof(buffer))) > 0) {
        buffer[read_size] = '\0';

        // 채팅 메시지를 모든 클라이언트에게 방송
        char broadcast_message[150];
        snprintf(broadcast_message, sizeof(broadcast_message), "%s: %s", client.username, buffer);
        broadcast_message_to_all(broadcast_message);
    }

    // 클라이언트가 연결을 종료한 경우
    printf("클라이언트가 연결을 종료했습니다: %s\n", client.username);

    // 사용자가 퇴장했음을 알림
    char leave_message[150];
    snprintf(leave_message, sizeof(leave_message), "%s 님이 퇴장하셨습니다.", client.username);
    broadcast_message_to_all(leave_message);

    // 클라이언트 소켓 닫기
    close(client.socket);
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

        // 새로운 클라이언트를 등록
        Client new_client = {client_socket, ""};
        for (int i = 0; i < MAX_CLIENTS; ++i) {
            if (clients[i].socket == 0) {
                clients[i] = new_client;
                break;
            }
        }

        if (fork() == 0) {
            close(server_socket);
            handle_client(new_client);
            close(client_socket);
            exit(EXIT_SUCCESS);
        } else {
            close(client_socket);
        }
    }

    close(server_socket);

    return 0;
}

