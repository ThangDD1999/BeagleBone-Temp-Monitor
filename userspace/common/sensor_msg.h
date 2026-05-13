#ifndef SENSOR_MSG_H
#define SENSOR_MSG_H

#include <time.h>

/* 1. Data Structure Definition */
typedef struct {
    float temperature;
    float humidity;
    time_t timestamp;
} sensor_msg_t;

/* 2. Communication and System Macros */
#define MQ_NAME         "/dht11_queue"
#define MQ_MAX_MSGS     10
#define MQ_MSG_SIZE     sizeof(sensor_msg_t)  /* Valid size defined after struct */

#define ALERT_THRESHOLD 40.0f
#define COOLDOWN_SECS   20      
#define POLL_INTERVAL   10       /* Sampling interval in seconds */
#define DEV_DHT11       "/dev/my_dht11"
#define HTTP_PORT       8080

#endif