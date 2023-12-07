#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <fcntl.h>
#include <libgen.h>

#define PORT 8080
#define BUFFER_SIZE 1024

GtkWidget *chat_view;
GtkWidget *input_entry;

int client_socket;

// 텍스트를 채팅창에 추가하는 함수
void append_text_to_chat(const char *text) {
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(chat_view));
    GtkTextIter iter;
    gtk_text_buffer_get_end_iter(buffer, &iter);
    gtk_text_buffer_insert(buffer, &iter, text, -1);
    gtk_text_buffer_insert(buffer, &iter, "\n", -1);
}

// 메시지 수신 콜백 함수
gboolean receive_message(GIOChannel *source, GIOCondition condition, gpointer data) {
    char buffer[BUFFER_SIZE];
    ssize_t read_size = read(client_socket, buffer, sizeof(buffer));
    if (read_size > 0) {
        buffer[read_size] = '\0';
        append_text_to_chat(buffer);
    } else if (read_size == 0) {
        // 클라이언트 연결이 종료된 경우
        g_io_channel_shutdown(source, TRUE, NULL);
        append_text_to_chat("서버 연결이 종료되었습니다.");
    } else {
        perror("메시지 수신 실패");
    }
    return TRUE;
}

// 파일 전송 함수
void send_file(int server_socket, const char *file_path) {
    char buffer[BUFFER_SIZE];
    ssize_t read_size;

    int file_fd = open(file_path, O_RDONLY);
    if (file_fd == -1) {
        perror("파일 열기 실패");
        return;
    }

    // 파일 전송 요청 메시지 전송
    write(server_socket, "FILE_TRANSFER_REQUEST", sizeof("FILE_TRANSFER_REQUEST"));

    // 파일 이름 길이 및 파일 이름 전송
    char *file_name = g_path_get_basename(file_path);
    size_t file_name_length = strlen(file_name);
    write(server_socket, &file_name_length, sizeof(size_t));
    write(server_socket, file_name, file_name_length);

    // 파일 크기 전송
    off_t file_size = lseek(file_fd, 0, SEEK_END);
    write(server_socket, &file_size, sizeof(off_t));

    // 파일 데이터 전송
    lseek(file_fd, 0, SEEK_SET);
    while ((read_size = read(file_fd, buffer, sizeof(buffer))) > 0) {
        write(server_socket, buffer, read_size);
    }

    close(file_fd);
}

// 메시지 전송 함수
void send_message(GtkWidget *widget, gpointer data) {
    const char *message = gtk_entry_get_text(GTK_ENTRY(input_entry));
    if (strlen(message) > 0) {
        if (strncmp(message, "/file", 5) == 0) {
            // 파일 전송 요청 시
            char file_path[256];
            strncpy(file_path, message + 6, sizeof(file_path) - 1);
            file_path[sizeof(file_path) - 1] = '\0';
            send_file(client_socket, file_path);
        } else {
            write(client_socket, message, strlen(message));
        }
        gtk_entry_set_text(GTK_ENTRY(input_entry), "");
    }
}

// 메인 함수
int main(int argc, char **argv) {
    gtk_init(&argc, &argv);

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Chat Client");
    gtk_window_set_default_size(GTK_WINDOW(window), 400, 300);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    chat_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(chat_view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(chat_view), GTK_WRAP_WORD_CHAR);

    gtk_container_add(GTK_CONTAINER(scrolled_window), chat_view);

    input_entry = gtk_entry_new();
    GtkWidget *send_button = gtk_button_new_with_label("Send");

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_box_pack_start(GTK_BOX(vbox), scrolled_window, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), input_entry, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), send_button, FALSE, FALSE, 0);

    gtk_container_add(GTK_CONTAINER(window), vbox);

    g_signal_connect(send_button, "clicked", G_CALLBACK(send_message), NULL);

    // 서버에 연결
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        perror("소켓 생성 실패");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_addr.sin_port = htons(PORT);

    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("서버에 연결 실패");
        exit(EXIT_FAILURE);
    }

    // 소켓을 GIOChannel로 등록하여 비동기적으로 메시지를 수신
    GIOChannel *channel = g_io_channel_unix_new(client_socket);
    g_io_add_watch(channel, G_IO_IN, receive_message, NULL);

    gtk_widget_show_all(window);

    gtk_main();

    // 프로그램 종료 시 소켓 닫기
    close(client_socket);

    return 0;
}

