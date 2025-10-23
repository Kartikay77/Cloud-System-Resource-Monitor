# ☁️ Cloud System Resource Monitor

A lightweight **multi-threaded Linux monitoring utility** written in **C** that displays and logs  
real-time **CPU usage**, **memory consumption**, and **process count** — designed for cloud and DevOps environments.

---

## 📸 Demo
# UBUNTU 24
![Ubuntu Monitor Output](https://github.com/Kartikay77/Cloud-System-Resource-Monitor/blob/main/Ubuntu_24_1.jpeg)
![Ubuntu Monitor Output](https://github.com/Kartikay77/Cloud-System-Resource-Monitor/blob/main/Ubuntu_24_2.jpeg)
![Ubuntu Monitor Output](https://github.com/Kartikay77/Cloud-System-Resource-Monitor/blob/main/Ubuntu_24_3.jpeg)
![Ubuntu Monitor Output](https://github.com/Kartikay77/Cloud-System-Resource-Monitor/blob/main/Ubuntu_24_4.jpeg)


# UBUNTU 25
![Ubuntu Monitor Output](https://github.com/Kartikay77/Cloud-System-Resource-Monitor/blob/main/Ubuntu_25_1.jpeg)
![Ubuntu Monitor Output](https://github.com/Kartikay77/Cloud-System-Resource-Monitor/blob/main/Ubuntu_25_2.jpeg)
![Ubuntu Monitor Output](https://github.com/Kartikay77/Cloud-System-Resource-Monitor/blob/main/Ubuntu_25_3.jpeg)
---

## ⚙️ Features
- Reads metrics directly from the `/proc` filesystem (`/proc/stat`, `/proc/meminfo`)
- Monitors:
  - ✅ CPU utilization (%)
  - ✅ Memory usage (used/total)
  - ✅ Number of active processes
- Multi-threaded design using **POSIX threads**
- **Graceful SIGINT** (Ctrl+C) termination
- Optional **logging** to `system_stats.log`
- Configurable **update interval**

---

## 🚀 Compilation & Usage

```bash
# 1️⃣ Clone the repository
git clone https://github.com/Kartikay77/Cloud-System-Resource-Monitor.git
cd Cloud-System-Resource-Monitor
```
```
# 2️⃣ Compile using gcc with pthread
gcc -pthread Optium_Main_Test.c -o cloud_monitor
```
```
# 3️⃣ Run the monitor (default interval = 1s)
./cloud_monitor
```
```
# 4️⃣ Run with custom interval and logging
./cloud_monitor --interval 2 --log
```

#🖥️ Example Output
System Resource Monitor
-------------------------------------------------
CPU Usage:       46.9%
Memory Usage:    2.0 GB / 3.8 GB (52.9%)
Running Processes: 208
-------------------------------------------------
(Updating every 1 second... Press Ctrl+C to exit)

#📄 Example Log File (system_stats.log)
[2025-08-23 19:07:49] CPU: 10.9%, Memory: 2.1GB/3.8GB, Processes: 208
[2025-08-23 19:07:50] CPU: 0.5%,  Memory: 2.1GB/3.8GB, Processes: 208
[2025-08-23 19:07:55] CPU: 2.5%,  Memory: 2.1GB/3.8GB, Processes: 207

#🧠 Architecture
 ┌──────────────────────────────────────────────────────────┐
 │                    Cloud-System-Monitor                  │
 ├──────────────────────────────────────────────────────────┤
 │ Threads:                                                 │
 │  • CPU Monitor Thread → Reads /proc/stat                 │
 │  • Memory Monitor Thread → Reads /proc/meminfo           │
 │  • Process Counter Thread → Scans /proc/ directories     │
 │ Synchronization: Mutex locks & safe printing             │
 │ Signal Handling: Graceful exit on SIGINT                 │
 └──────────────────────────────────────────────────────────┘

# 🧰 Tech Stack
Languages: C (POSIX threads)
Platform: Linux / Ubuntu
System APIs: /proc, pthread, signal.h, stdio.h
Category: System Monitoring / Cloud Observability

# 🧩 Potential Extensions
Integrate with Datadog API or Prometheus Pushgateway
Add YAML/JSON output for structured telemetry
Build dashboard with Python + Matplotlib for log visualization
