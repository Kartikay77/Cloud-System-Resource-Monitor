#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <dirent.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/syscall.h>

#ifndef USE_GETDENTS64
# if defined(__linux__) && defined(SYS_getdents64)
#  define USE_GETDENTS64 1
# else
#  define USE_GETDENTS64 0
# endif
#endif
#ifdef DISABLE_GETDENTS64
# undef USE_GETDENTS64
# define USE_GETDENTS64 0
#endif

// Shared statistics structure
typedef struct {
    double cpu_percent;
    unsigned long mem_total;
    unsigned long mem_free;
    int proc_count;
} SysStats;

static SysStats metrics = {0};
static pthread_mutex_t metrics_lock = PTHREAD_MUTEX_INITIALIZER;
static volatile sig_atomic_t keep_running = 1;

static void stop_signal_handler(int sig) {
    (void)sig;
    keep_running = 0;
}

static void setup_signals(void) {
    struct sigaction sa = {0};
    sa.sa_handler = stop_signal_handler;
    sigaction(SIGINT, &sa, NULL);
}

static void get_monotonic_now(struct timespec *t) {
    clock_gettime(CLOCK_MONOTONIC, t);
}

static void normalize_timespec(struct timespec *ts) {
    if (ts->tv_nsec >= 1000000000L) {
        ts->tv_sec += ts->tv_nsec / 1000000000L;
        ts->tv_nsec %= 1000000000L;
    } else if (ts->tv_nsec < 0) {
        long s = (-ts->tv_nsec + 999999999L) / 1000000000L;
        ts->tv_sec -= s;
        ts->tv_nsec += s * 1000000000L;
    }
}

static void timespec_add(struct timespec *ts, const struct timespec *delta) {
    ts->tv_sec += delta->tv_sec;
    ts->tv_nsec += delta->tv_nsec;
    normalize_timespec(ts);
}

static void sleep_until(const struct timespec *deadline) {
    while (keep_running) {
        int err = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, deadline, NULL);
        if (err == 0 || (err == EINTR && !keep_running)) break;
    }
}

static struct timespec double_to_timespec(double seconds) {
    if (seconds <= 0.0) seconds = 0.1;
    long long ns = (long long)(seconds * 1e9);
    struct timespec ts = {
        .tv_sec = ns / 1000000000LL,
        .tv_nsec = ns % 1000000000LL
    };
    return ts;
}

static void* monitor_cpu(void *arg) {
    struct timespec interval = *(struct timespec*)arg;
    struct timespec next_check;
    get_monotonic_now(&next_check);
    timespec_add(&next_check, &interval);

    FILE *fp;
    char buffer[512];
    unsigned long long prev_idle = 0, prev_total = 0;

    while (keep_running) {
        fp = fopen("/proc/stat", "r");
        if (!fp) break;
        if (!fgets(buffer, sizeof(buffer), fp)) {
            fclose(fp);
            continue;
        }
        fclose(fp);

        unsigned long long vals[8] = {0};
        sscanf(buffer, "cpu  %llu %llu %llu %llu %llu %llu %llu %llu",
               &vals[0], &vals[1], &vals[2], &vals[3], &vals[4], &vals[5], &vals[6], &vals[7]);

        unsigned long long idle = vals[3] + vals[4];
        unsigned long long total = 0;
        for (int i = 0; i < 8; ++i) total += vals[i];

        unsigned long long delta_total = total - prev_total;
        unsigned long long delta_idle = idle - prev_idle;

        double cpu_pct = (delta_total > 0) ? (double)(delta_total - delta_idle) * 100.0 / delta_total : 0.0;

        pthread_mutex_lock(&metrics_lock);
        metrics.cpu_percent = cpu_pct;
        pthread_mutex_unlock(&metrics_lock);

        prev_idle = idle;
        prev_total = total;

        sleep_until(&next_check);
        timespec_add(&next_check, &interval);
    }

    return NULL;
}

static void* monitor_memory(void *arg) {
    struct timespec interval = *(struct timespec*)arg;
    struct timespec next_check;
    get_monotonic_now(&next_check);
    timespec_add(&next_check, &interval);

    while (keep_running) {
        FILE *fp = fopen("/proc/meminfo", "r");
        if (!fp) break;

        char line[256];
        unsigned long total = 0, available = 0;

        while (fgets(line, sizeof(line), fp)) {
            if (sscanf(line, "MemTotal: %lu kB", &total) == 1) continue;
            if (sscanf(line, "MemAvailable: %lu kB", &available) == 1) continue;
        }
        fclose(fp);

        pthread_mutex_lock(&metrics_lock);
        metrics.mem_total = total;
        metrics.mem_free = available;
        pthread_mutex_unlock(&metrics_lock);

        sleep_until(&next_check);
        timespec_add(&next_check, &interval);
    }

    return NULL;
}

static bool is_numeric(const char *name) {
    if (!name || !*name) return false;
    while (*name) {
        if (!isdigit((unsigned char)*name)) return false;
        ++name;
    }
    return true;
}

static void* monitor_processes(void *arg) {
    struct timespec interval = *(struct timespec*)arg;
    struct timespec next_check;
    get_monotonic_now(&next_check);
    timespec_add(&next_check, &interval);

    while (keep_running) {
        DIR *dir = opendir("/proc");
        if (!dir) break;

        int count = 0;
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (is_numeric(entry->d_name)) count++;
        }
        closedir(dir);

        pthread_mutex_lock(&metrics_lock);
        metrics.proc_count = count;
        pthread_mutex_unlock(&metrics_lock);

        sleep_until(&next_check);
        timespec_add(&next_check, &interval);
    }

    return NULL;
}

int main(int argc, char **argv) {
    double interval_secs = 1.0;
    bool log_enabled = false;
    int log_fd = -1;

    for (int i = 1; i < argc; ++i) {
        if ((strcmp(argv[i], "--interval") == 0 || strcmp(argv[i], "-i") == 0) && i+1 < argc) {
            interval_secs = atof(argv[++i]);
            if (interval_secs <= 0.0) interval_secs = 1.0;
        } else if (strcmp(argv[i], "--log") == 0 || strcmp(argv[i], "-l") == 0) {
            log_enabled = true;
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            return 1;
        }
    }

    if (log_enabled) {
        log_fd = open("system_stats.log", O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644);
        if (log_fd == -1) {
            perror("open(system_stats.log)");
            return 1;
        }
    }

    setup_signals();

    pthread_t th_cpu, th_mem, th_proc;
    struct timespec interval = double_to_timespec(interval_secs);

    pthread_create(&th_cpu, NULL, monitor_cpu, &interval);
    pthread_create(&th_mem, NULL, monitor_memory, &interval);
    pthread_create(&th_proc, NULL, monitor_processes, &interval);

    while (keep_running) {
        sleep(interval_secs);

        pthread_mutex_lock(&metrics_lock);
        double cpu = metrics.cpu_percent;
        unsigned long total = metrics.mem_total;
        unsigned long free = metrics.mem_free;
        int procs = metrics.proc_count;
        pthread_mutex_unlock(&metrics_lock);

        double used_gb = (total > free) ? (double)(total - free) / (1024.0 * 1024.0) : 0.0;
        double total_gb = (double)total / (1024.0 * 1024.0);
        double percent_used = (total > 0) ? (used_gb / total_gb) * 100.0 : 0.0;

        printf("\033[H\033[J");
        printf("System Resource Monitor\n");
        printf("-------------------------------------------------\n");
        printf("CPU Usage:       %.1f%%\n", cpu);
        printf("Memory Usage:    %.1f GB / %.1f GB (%.1f%%)\n", used_gb, total_gb, percent_used);
        printf("Running Processes: %d\n", procs);
        printf("-------------------------------------------------\n");
        if (interval_secs == (double)(long long)interval_secs) {
            printf("(Updating every %.0f second%s... Press Ctrl+C to exit)\n", interval_secs, interval_secs == 1.0 ? "" : "s");
        } else {
            printf("(Updating every %.3f seconds... Press Ctrl+C to exit)\n", interval_secs);
        }
        fflush(stdout);

        if (log_enabled && log_fd != -1) {
            time_t now = time(NULL);
            struct tm tm_now;
            localtime_r(&now, &tm_now);
            char tbuf[32];
            strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", &tm_now);

            char log_line[256];
            int len = snprintf(log_line, sizeof(log_line),
                "[%s] CPU: %.1f%%, Memory: %.1fGB/%.1fGB, Processes: %d\n",
                tbuf, cpu, used_gb, total_gb, procs);

            if (len > 0 && len < (int)sizeof(log_line)) {
                ssize_t written = write(log_fd, log_line, len);
                (void)written;
            }
        }
    }

    pthread_join(th_cpu, NULL);
    pthread_join(th_mem, NULL);
    pthread_join(th_proc, NULL);
    if (log_fd != -1) close(log_fd);

    return 0;
}
