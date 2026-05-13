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
#include <sys/resource.h>
#include "../common/sensor_msg.h"

/* 30 mins x 3 samples/min (20s interval) */
#define HISTORY_SIZE 90  

typedef struct {
    sensor_msg_t data[HISTORY_SIZE];
    int head;   /* Next write position */
    int count;  /* Current number of elements */
} history_buffer_t;

static history_buffer_t history = { .head = 0, .count = 0 };

/* Telegram Configuration - Replace with your actual credentials */
#define TELEGRAM_TOKEN  "YOUR_TELEGRAM_BOT_TOKEN"
#define TELEGRAM_CHATID "YOUR_CHAT_ID"

/* Shared data between MQ thread and HTTP thread */
static sensor_msg_t latest_msg;
static pthread_mutex_t data_mutex = PTHREAD_MUTEX_INITIALIZER;
static int has_data = 0; 

static void send_telegram(float temp)
{
    static time_t last_sent = 0;
    time_t now = time(NULL);

    /* Anti-spam cooldown */
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
        printf("[server] Telegram alert sent successfully!\n");
        last_sent = now;
    } else {
        fprintf(stderr, "[server] Failed to send Telegram alert\n");
    }
}

static void add_to_history(sensor_msg_t msg) {
    pthread_mutex_lock(&data_mutex);
    
    /* Circular buffer logic */
    history.data[history.head] = msg;
    history.head = (history.head + 1) % HISTORY_SIZE;
    
    if (history.count < HISTORY_SIZE) {
        history.count++;
    }
    
    /* Update latest message cache */
    memcpy(&latest_msg, &msg, sizeof(msg));
    has_data = 1;
    
    pthread_mutex_unlock(&data_mutex);
}

/* ── HTTP Handler ── */
static void handle_client(int client_fd) {
    char req[1024];
    ssize_t req_len = read(client_fd, req, sizeof(req) - 1);
    if (req_len <= 0) { close(client_fd); return; }
    req[req_len] = '\0';

    /* 1. Handle API Request for History Data */
    if (strstr(req, "GET /api/data")) {
        char *json = malloc(1024 * 10); 
        char temp_item[128];
        
        strcpy(json, "{\"history\": [");
        
        pthread_mutex_lock(&data_mutex);
        for (int i = 0; i < history.count; i++) {
            /* Calculate index from oldest to newest */
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
         "Content-Type: application/json; charset=utf-8\r\n"
         "Access-Control-Allow-Origin: *\r\n\r\n%s", json);
        write(client_fd, response, strlen(response));
        free(json);
    } 
    /* 2. Serve Dashboard Interface (index.html) */
    else {
        const char *html_header = "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n\r\n";
        write(client_fd, html_header, strlen(html_header));
        
        FILE *f = fopen("index.html", "r");
        if (f) {
            char buf[1024];
            size_t n;
            while ((n = fread(buf, 1, sizeof(buf), f)) > 0) write(client_fd, buf, n);
            fclose(f);
        } else {
            const char *error = "<h1>404 Dashboard Not Found</h1><p>Please upload index.html to BeagleBone Black</p>";
            write(client_fd, error, strlen(error));
        }
    }
    close(client_fd);
}

/* ── HTTP Server Thread ── */
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
        perror("[server] Bind failed");
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

/* ── Message Queue Consumer Thread ── */
static void *mq_thread(void *arg)
{
    struct mq_attr attr = {
        .mq_flags = 0,
        .mq_maxmsg = MQ_MAX_MSGS,
        .mq_msgsize = MQ_MSG_SIZE,
        .mq_curmsgs = 0
    };

    /* Open queue with attributes matching the Collector */
    mqd_t mq = mq_open(MQ_NAME, O_RDONLY | O_CREAT, 0644, &attr);
    if (mq == (mqd_t)-1) {
        perror("[server] mq_open failed");
        exit(EXIT_FAILURE);
    }

    printf("[server] Waiting for data on MQ: %s\n", MQ_NAME);
    fflush(stdout);

    while (1) {
        sensor_msg_t msg;
        /* Block until new data arrives */
        ssize_t n = mq_receive(mq, (char *)&msg, MQ_MSG_SIZE, NULL);
        
        if (n >= 0) {
            char timestr[32];
            struct tm *tm_info = localtime(&msg.timestamp);
            strftime(timestr, sizeof(timestr), "%Y-%m-%d %H:%M:%S", tm_info);

            printf("[server] %s | TEMP: %.1fC | HUM: %.1f%%\n", timestr, msg.temperature, msg.humidity);
            fflush(stdout);

            add_to_history(msg);

            if (msg.temperature >= ALERT_THRESHOLD) {
                send_telegram(msg.temperature);
            }
            usleep(10000); /* 10ms sleep */
        }
    }
    return NULL;
}

int main(void)
{
    /* Set lower priority for Server process (Nice value: 10) */
    setpriority(PRIO_PROCESS, 0, 10);

    printf("[server] Starting system with optimized priority...\n");
    memset(&latest_msg, 0, sizeof(latest_msg));

    pthread_t tid1, tid2;
    pthread_create(&tid1, NULL, http_thread, NULL);
    pthread_create(&tid2, NULL, mq_thread, NULL);

    /* Keep threads running */
    pthread_join(tid1, NULL);
    pthread_join(tid2, NULL);

    return 0;
}