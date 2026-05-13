#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <mqueue.h>
#include <time.h>
#include <errno.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "../common/sensor_msg.h"
#include <sys/resource.h>


#define HISTORY_SIZE 90  /* 30 phút x 3 mẫu/phút (20s/mẫu) */

typedef struct {
    sensor_msg_t data[HISTORY_SIZE];
    int head;   /* Vị trí ghi tiếp theo */
    int count;  /* Số lượng phần tử hiện có */
} history_buffer_t;

static history_buffer_t history = { .head = 0, .count = 0 };
/* Vẫn dùng data_mutex có sẵn của bạn để bảo vệ history */


/* Thông tin Telegram - Thắng thay token thật vào đây */
#define TELEGRAM_TOKEN  "8669982112:AAFZ7s5ff-L-rpxiW8oMraqNKV0iwwFZZmk"
#define TELEGRAM_CHATID "8772537568"

/* Dữ liệu dùng chung giữa luồng MQ và luồng HTTP */
static sensor_msg_t latest_msg;
static pthread_mutex_t data_mutex = PTHREAD_MUTEX_INITIALIZER;
static int has_data = 0; // Cờ kiểm tra xem đã có dữ liệu chưa

static void send_telegram(float temp)
{
    static time_t last_sent = 0;
    time_t now = time(NULL);

    if (difftime(now, last_sent) < COOLDOWN_SECS)
        return;

    char cmd[512];
    snprintf(cmd, sizeof(cmd),
        "wget -q -O /dev/null "
        "--post-data=\"chat_id=%s"
        "&text=WARNING: Temperature %.1fC exceeded threshold!\" "
        "\"https://api.telegram.org/bot%s/sendMessage\"",
        TELEGRAM_CHATID, temp, TELEGRAM_TOKEN);

    if (system(cmd) == 0) {
        printf("[server] Telegram alert sent!\n");
        last_sent = now;
    } else {
        fprintf(stderr, "[server] Telegram failed\n");
    }
}

static void add_to_history(sensor_msg_t msg) {
    pthread_mutex_lock(&data_mutex);
    
    history.data[history.head] = msg;
    history.head = (history.head + 1) % HISTORY_SIZE;
    
    if (history.count < HISTORY_SIZE) {
        history.count++;
    }
    
    /* Cập nhật latest_msg cũ của bạn */
    memcpy(&latest_msg, &msg, sizeof(msg));
    has_data = 1;
    
    pthread_mutex_unlock(&data_mutex);
}

// /* ── HTTP Handler ── */
// static void handle_client(int client_fd)
// {
//     char req[512];
//     read(client_fd, req, sizeof(req) - 1);

//     sensor_msg_t msg;
//     int data_available;

//     pthread_mutex_lock(&data_mutex);
//     memcpy(&msg, &latest_msg, sizeof(msg));
//     data_available = has_data;
//     pthread_mutex_unlock(&data_mutex);

//     char json[256];
//     if (data_available) {
//         char timestr[32];
//         struct tm *tm_info = localtime(&msg.timestamp);
//         strftime(timestr, sizeof(timestr), "%Y-%m-%d %H:%M:%S", tm_info);

//         snprintf(json, sizeof(json),
//                  "{\"status\":\"ok\",\"temperature\":%.1f,\"humidity\":%.1f,\"timestamp\":\"%s\"}",
//                  msg.temperature, msg.humidity, timestr);
//     } else {
//         snprintf(json, sizeof(json), "{\"status\":\"error\",\"message\":\"No data available\"}");
//     }

//     char response[1024];
//     snprintf(response, sizeof(response),
//              "HTTP/1.1 200 OK\r\n"
//              "Content-Type: application/json\r\n"
//              "Access-Control-Allow-Origin: *\r\n"
//              "Content-Length: %zu\r\n"
//              "\r\n"
//              "%s",
//              strlen(json), json);

//     write(client_fd, response, strlen(response));
//     close(client_fd);
// }

static void handle_client(int client_fd) {
    char req[1024];
    ssize_t req_len = read(client_fd, req, sizeof(req) - 1);
    if (req_len <= 0) { close(client_fd); return; }
    req[req_len] = '\0';

    /* 1. Xử lý API lấy dữ liệu lịch sử */
    if (strstr(req, "GET /api/data")) {
        char *json = malloc(1024 * 10); // Cấp phát đủ cho 90 mẫu
        char temp_item[128];
        
        strcpy(json, "{\"history\": [");
        
        pthread_mutex_lock(&data_mutex);
        for (int i = 0; i < history.count; i++) {
            /* Tính toán chỉ số thực tế trong vòng tròn (từ cũ đến mới) */
            int idx = (history.head - history.count + i + HISTORY_SIZE) % HISTORY_SIZE;
            sensor_msg_t *m = &history.data[idx];
            
            snprintf(temp_item, sizeof(temp_item), 
                     "{\"t\":%.1f,\"h\":%.1f,\"s\":%ld}%s",
                     m->temperature, m->humidity, (long)m->timestamp,
                     (i == history.count - 1) ? "" : ",");
            strcat(json, temp_item);
        }
        pthread_mutex_unlock(&data_mutex);
        strcat(json, "]}");

        char response[1024 * 11];
        snprintf(response, sizeof(response),
         "HTTP/1.1 200 OK\r\n"
         "Content-Type: application/json; charset=utf-8\r\n" // Thêm charset ở đây
         "Access-Control-Allow-Origin: *\r\n\r\n%s", json);
        write(client_fd, response, strlen(response));
        free(json);
    } 
    /* 2. Xử lý trả về giao diện Dashboard (index.html) */
    else {
        /* Bạn có thể đọc file index.html từ disk hoặc nhúng string tại đây */
        const char *html_header = "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n\r\n";
        write(client_fd, html_header, strlen(html_header));
        
        // Gợi ý: Gửi file index.html (Thắng cần tạo file này cùng thư mục)
        FILE *f = fopen("index.html", "r");
        if (f) {
            char buf[1024];
            size_t n;
            while ((n = fread(buf, 1, sizeof(buf), f)) > 0) write(client_fd, buf, n);
            fclose(f);
        } else {
            const char *error = "<h1>404 Dashboard Not Found</h1><p>Vui lòng upload index.html len BBB</p>";
            write(client_fd, error, strlen(error));
        }
    }
    close(client_fd);
}

/* ── Luồng HTTP Server ── */
static void *http_thread(void *arg)
{
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(HTTP_PORT)
    };

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    
    listen(server_fd, 10);
    printf("[server] HTTP API ready on port %d\n", HTTP_PORT);
    fflush(stdout);

    while (1) {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd >= 0) handle_client(client_fd);
    }
    return NULL;
}

/* ── Luồng nhận Message Queue ── */
static void *mq_thread(void *arg)
{
    struct mq_attr attr = {
        .mq_flags = 0,
        .mq_maxmsg = MQ_MAX_MSGS,
        .mq_msgsize = MQ_MSG_SIZE,
        .mq_curmsgs = 0
    };

    // Mở queue với thuộc tính khớp hoàn toàn với Collector
    mqd_t mq = mq_open(MQ_NAME, O_RDONLY | O_CREAT, 0644, &attr);
    if (mq == (mqd_t)-1) {
        perror("mq_open");
        exit(EXIT_FAILURE);
    }

    printf("[server] waiting for data from MQ: %s\n", MQ_NAME);
    fflush(stdout);

    while (1) {
        sensor_msg_t msg;
        // mq_receive sẽ block ở đây cho đến khi có dữ liệu mới
        ssize_t n = mq_receive(mq, (char *)&msg, MQ_MSG_SIZE, NULL);
        
        if (n >= 0) {
            /* Thêm timestamp */
            char timestr[32];
            struct tm *tm_info = localtime(&msg.timestamp);
            strftime(timestr, sizeof(timestr), "%Y-%m-%d %H:%M:%S", tm_info);

            printf("[server] %s TEMP=%.1fC HUM=%.1f%%\n", timestr, msg.temperature, msg.humidity);
            fflush(stdout);

            add_to_history(msg);

            if (msg.temperature >= ALERT_THRESHOLD) {
                send_telegram(msg.temperature);
            }
            usleep(10000); // Ngủ 10ms
        }
    }
    return NULL;
}

int main(void)
{
    /* Hạ độ ưu tiên của Server (10) */
    setpriority(PRIO_PROCESS, 0, 10);

    printf("[server] starting with low priority...\n");

    printf("[server] initializing...\n");
    memset(&latest_msg, 0, sizeof(latest_msg));

    pthread_t tid1, tid2;
    pthread_create(&tid1, NULL, http_thread, NULL);
    pthread_create(&tid2, NULL, mq_thread, NULL);

    pthread_join(tid1, NULL);
    pthread_join(tid2, NULL);

    return 0;
}