#ifndef SENSOR_MSG_H
#define SENSOR_MSG_H

#include <time.h>

/* 1. Định nghĩa kiểu dữ liệu trước */
typedef struct {
    float temperature;
    float humidity;
    time_t timestamp;
} sensor_msg_t;

/* 2. Sau đó mới định nghĩa các Macro sử dụng kiểu dữ liệu đó */
#define MQ_NAME         "/dht11_queue"
#define MQ_MAX_MSGS     10
#define MQ_MSG_SIZE     sizeof(sensor_msg_t)  /* Bây giờ sizeof đã có giá trị chuẩn */

#define ALERT_THRESHOLD 40.0f
#define COOLDOWN_SECS   20      
#define POLL_INTERVAL   10       /* Thắng có thể để 10 hoặc 20 tùy ý */
#define DEV_DHT11       "/dev/my_dht11"
#define HTTP_PORT       8080

#endif