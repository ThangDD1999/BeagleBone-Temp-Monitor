#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <mqueue.h>
#include <time.h>
#include <errno.h>
#include "../common/sensor_msg.h"
#include <sys/resource.h>

#define MAX_RETRIES     5      
#define RETRY_DELAY_SEC 3  

static int read_sensor_fresh(float *temp, float *hum) {
    int fd = open(DEV_DHT11, O_RDONLY);
    if (fd < 0) return -1;

    char buf[128];
    int n = read(fd, buf, sizeof(buf) - 1);
    close(fd); 

    if (n <= 0) return -1;
    buf[n] = '\0';

    if (strstr(buf, "Error")) return -1;

    if (sscanf(buf, "Humidity: %f%% Temperature: %fC", hum, temp) == 2) {
        return 0;
    }
    return -1;
}

int main(void) {
    /* 1. Thiết lập độ ưu tiên cực cao để chiếm quyền CPU khi đọc sensor */
    if (setpriority(PRIO_PROCESS, 0, -20) < 0) {
        perror("setpriority failed");
    }

    printf("[collector] starting with the highest priority...\n");

    /* 2. Cấu hình thuộc tính Message Queue đồng bộ với Server */
    struct mq_attr attr = {
        .mq_flags = 0,
        .mq_maxmsg = MQ_MAX_MSGS,
        .mq_msgsize = MQ_MSG_SIZE,
        .mq_curmsgs = 0
    };

    /* 3. Mở hoặc tạo Queue với cấu hình attr chuẩn */
    mqd_t mq = mq_open(MQ_NAME, O_CREAT | O_WRONLY, 0644, &attr);
    if (mq == (mqd_t)-1) {
        perror("[collector] mq_open failed");
        exit(EXIT_FAILURE);
    }
    
    while (1) {
        sensor_msg_t msg;
        int success = 0;

        /* Cơ chế Retry để đối phó với nhiễu từ các tiến trình khác (SSH, Server) */
        for (int i = 0; i < MAX_RETRIES; i++) {
            if (read_sensor_fresh(&msg.temperature, &msg.humidity) == 0) {
                success = 1;
                break;
            }
            printf("[collector] Attempt %d failed, waiting %ds...\n", i+1, RETRY_DELAY_SEC);
            sleep(RETRY_DELAY_SEC); 
        }

        if (success) {
            msg.timestamp = time(NULL);
            char timestr[32];
            
            strftime(timestr, sizeof(timestr), "%Y-%m-%d %H:%M:%S", localtime(&msg.timestamp));

            printf("[collector] %s TEMP=%.1f°C HUM=%.1f%%\n", timestr, msg.temperature, msg.humidity);
            
            /* Gửi dữ liệu vào Queue */
            if (mq_send(mq, (const char *)&msg, sizeof(msg), 0) < 0) {
                perror("[collector] mq_send error");
            }
        }

        /* Nghỉ 10s để giải phóng CPU hoàn toàn cho Server xử lý Web/Telegram */
        sleep(POLL_INTERVAL); 
    }

    mq_close(mq);
    return 0;
}