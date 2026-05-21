# 🌡️ BeagleBone Black Temperature Monitoring System

[![Platform](https://img.shields.io/badge/Platform-BeagleBone%20Black-orange.svg)]()
[![Language](https://img.shields.io/badge/Language-C-blue.svg)]()
[![Kernel](https://img.shields.io/badge/Kernel-5.x-green.svg)]()

A professional IoT and Embedded Linux project featuring a custom DHT11 kernel driver and a multi-process user-space application. This system provides real-time monitoring, a web dashboard, and automated alerts via Telegram, accessible globally through an Ngrok tunnel.

---

## 🏗️ System Architecture
The project follows a robust architecture to ensure precision and reliability:

* **Kernel-space**: Custom DHT11 character driver utilizing bit-banging for 1-wire communication and Device Tree Overlays.
* **User-space**: Multi-threaded architecture communicating via **POSIX Message Queues**.
    * **Collector**: High-priority process (`nice -20`) for precise sensor data acquisition.
    * **Server**: Low-priority process managing the HTTP API, Web Dashboard, and Telegram notifications.
* **Remote Access**: Automated Ngrok tunnel integration for global dashboard access.

---

## 📂 Project Structure
```bash
BBB-Temp-Monitor/
├── kernel-space/          # 📟 Hardware-level implementation
│   ├── drivers/           # DHT11 Kernel Driver (C source)
│   └── dtbo/              # Device Tree Overlay source
├── user-space/            # 💻 Application-level implementation
│   ├── apps/
│   │   ├── collector/     # Data acquisition service
│   │   └── server/        # Web server & Alert service
│   ├── common/            # Shared IPC headers (sensor_msg.h)
│   ├── bin/               # Compiled binaries (Git ignored)
│   ├── scripts/           # Deployment & Startup scripts (Ngrok config)
│   └── Makefile           # Recursive build system
├── images/                # 📸 Project screenshots (Dashboard, Telegram)
└── README.md
```

---

## 🚀 Key Features
* **Real-time Acquisition:** Precise 1-wire protocol handling in kernel-space.
* **Smart Alerting:** Instant Telegram notifications when temperature exceeds the threshold.
* **Data Visualization:** Real-time Web Dashboard with historical data tracking using Circular Buffers.
* **Remote Accessibility:** Automated Ngrok tunneling script to bypass NAT/Firewalls.
* **System Integrity:** Priority-based process scheduling to minimize sensor reading errors under load.

---

## 📸 Demo

| Web Dashboard | Telegram Alert |
|:---:|:---:|
| ![Web Dashboard](images/dashboard.png) | ![Telegram Alert](images/telegram.png) |

*(Note: Replace the image paths with your actual screenshot files in the `images/` directory)*

---

## 🛠️ Setup & Installation

### 1. Prerequisites
* BeagleBone Black board.
* ARM Cross-compiler toolchain (`arm-linux-gnueabihf-gcc`).

### 2. Configuration
Update your credentials in the following locations before building:
* **Telegram**: `user-space/apps/server/main.c` → `TELEGRAM_TOKEN`, `TELEGRAM_CHATID`.
* **Ngrok**: `user-space/scripts/tempmonitor` → `AUTH_TOKEN`.

### 3. Building
Navigate to the `user-space` directory and use the Makefile:
```bash
cd user-space
make clean
make
```
*All binaries will be generated in the `user-space/bin/` directory.*

### 4. Deployment
To start the entire system (Collector, Server, and Ngrok tunnel):
```bash
chmod +x scripts/tempmonitor
sudo ./scripts/tempmonitor start
```

---

## 👤 Author
**Thangdd** - Embedded Software Engineer specializing in Linux Kernel, Device Drivers, and Yocto Project.
