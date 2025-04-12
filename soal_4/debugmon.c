#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/types.h>
#include <ctype.h>
#include <signal.h>
#include <time.h>
#include <sys/stat.h>

void log_entry(const char *username, const char *procname, const char *status) {
    FILE *log = fopen("debugmon.log", "a");
    if (!log) return;

    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "[%d:%m:%Y]-%H:%M:%S", t);

    fprintf(log, "%s_%s_%s\n", timestamp, procname, status);
    fclose(log);
}

uid_t get_uid(const char *username) {
    struct passwd *pwd = getpwnam(username);
    if (!pwd) {
        fprintf(stderr, "User '%s' not found.\n", username);
        exit(EXIT_FAILURE);
    }
    return pwd->pw_uid;
}

void list_processes(const char *username) {
    uid_t target_uid = get_uid(username);
    DIR *proc = opendir("/proc");
    if (!proc) {
        perror("opendir /proc");
        exit(EXIT_FAILURE);
    }

    struct dirent *entry;
    while ((entry = readdir(proc)) != NULL) {
        if (!isdigit(entry->d_name[0])) continue;

        char status_path[256], stat_path[256], cmdline_path[256];
        snprintf(status_path, sizeof(status_path), "/proc/%s/status", entry->d_name);
        snprintf(stat_path, sizeof(stat_path), "/proc/%s/stat", entry->d_name);
        snprintf(cmdline_path, sizeof(cmdline_path), "/proc/%s/cmdline", entry->d_name);

        FILE *status_file = fopen(status_path, "r");
        if (!status_file) continue;

        uid_t uid = -1;
        char line[256];
        while (fgets(line, sizeof(line), status_file)) {
            if (strncmp(line, "Uid:", 4) == 0) {
                sscanf(line, "Uid:\t%d", &uid);
                break;
            }
        }
        fclose(status_file);
        if (uid != target_uid) continue;

        //Command
        FILE *cmdline_file = fopen(cmdline_path, "r");
        char cmdline[256] = "(unknown)";
        if (cmdline_file) {
            fgets(cmdline, sizeof(cmdline), cmdline_file);
            fclose(cmdline_file);
            if (strlen(cmdline) == 0) strcpy(cmdline, "(kernel)");
        }

        // CPU Usage
        FILE *stat_file = fopen(stat_path, "r");
        long utime = 0, stime = 0;
        if (stat_file) {
            char dummy[1024];
            fgets(dummy, sizeof(dummy), stat_file);
            char *token = strtok(dummy, " ");
            int i = 1;
            while (token) {
                if (i == 14) utime = atol(token);
                if (i == 15) stime = atol(token);
                token = strtok(NULL, " ");
                i++;
            }
            fclose(stat_file);
        }
        double cpu_usage = (utime + stime) / (double)sysconf(_SC_CLK_TCK);

        // Memory
        long mem_kb = 0;
        status_file = fopen(status_path, "r");
        if (status_file) {
            while (fgets(line, sizeof(line), status_file)) {
                if (strncmp(line, "VmRSS:", 6) == 0) {
                    sscanf(line, "VmRSS: %ld kB", &mem_kb);
                    break;
                }
            }
            fclose(status_file);
        }

        printf("PID: %s | CMD: %s | CPU: %.2f sec | MEM: %ld KB\n", entry->d_name, cmdline, cpu_usage, mem_kb);
    }

    closedir(proc);
}

void daemonize() {
    pid_t pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS);

    if (setsid() < 0) exit(EXIT_FAILURE);
    pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS);

    umask(0);
    chdir("/");

    freopen("/dev/null", "r", stdin);
    freopen("/dev/null", "w", stdout);
    freopen("/dev/null", "w", stderr);
}

void log_user_processes(const char *username) {
    uid_t target_uid = get_uid(username);
    DIR *proc = opendir("/proc");
    if (!proc) return;

    struct dirent *entry;
    while ((entry = readdir(proc)) != NULL) {
        if (!isdigit(entry->d_name[0])) continue;

        char status_path[256], cmdline_path[256];
        snprintf(status_path, sizeof(status_path), "/proc/%s/status", entry->d_name);
        snprintf(cmdline_path, sizeof(cmdline_path), "/proc/%s/cmdline", entry->d_name);

        FILE *status_file = fopen(status_path, "r");
        if (!status_file) continue;

        uid_t uid = -1;
        char line[256];
        while (fgets(line, sizeof(line), status_file)) {
            if (strncmp(line, "Uid:", 4) == 0) {
                sscanf(line, "Uid:\t%d", &uid);
                break;
            }
        }
        fclose(status_file);
        if (uid != target_uid) continue;

        FILE *cmdline_file = fopen(cmdline_path, "r");
        char cmdline[256] = "(unknown)";
        if (cmdline_file) {
            fgets(cmdline, sizeof(cmdline), cmdline_file);
            fclose(cmdline_file);
            if (strlen(cmdline) == 0) strcpy(cmdline, "(kernel)");
        }

        log_entry(username, cmdline, "RUNNING");
    }

    closedir(proc);
}

void run_daemon(const char *username) {
    pid_t pid = fork();

    if (pid < 0) {
        perror("Fork failed");
        exit(EXIT_FAILURE);
    }

    if (pid > 0) {
        printf("Daemon started with PID %d\n", pid);
        char pidfile[256];
        snprintf(pidfile, sizeof(pidfile), "/tmp/debugmon_%s.pid", username);
        FILE *pf = fopen(pidfile, "w");
        if (pf) {
            fprintf(pf, "%d\n", pid);
            fclose(pf);
        }
        return;
    }

    int iteration = 0;
    while (1) {
        FILE *debug = fopen("daemon_debug.log", "a");
        if (debug) {
            fprintf(debug, "Daemon loop %d for user %s\n", iteration++, username);
            fclose(debug);
        }

        log_user_processes(username);
        sleep(10);
    }
}

void stop_daemon(const char *username) {
    char pidfile[256];
    snprintf(pidfile, sizeof(pidfile), "/tmp/debugmon_%s.pid", username);
    FILE *pf = fopen(pidfile, "r");
    if (!pf) {
        fprintf(stderr, "Daemon not found.\n");
        return;
    }

    int pid;
    fscanf(pf, "%d", &pid);
    fclose(pf);

    if (kill(pid, SIGTERM) == 0) {
        printf("Daemon stopped for user %s.\n", username);
        remove(pidfile);
    } else {
        perror("Failed to stop daemon");
    }
}

void fail_user(const char *username) {
    uid_t target_uid = get_uid(username);
    DIR *proc = opendir("/proc");
    if (!proc) return;

    struct dirent *entry;
    while ((entry = readdir(proc)) != NULL) {
        if (!isdigit(entry->d_name[0])) continue;

        char status_path[256], cmdline_path[256];
        snprintf(status_path, sizeof(status_path), "/proc/%s/status", entry->d_name);
        snprintf(cmdline_path, sizeof(cmdline_path), "/proc/%s/cmdline", entry->d_name);

        FILE *status_file = fopen(status_path, "r");
        if (!status_file) continue;

        uid_t uid = -1;
        char line[256];
        while (fgets(line, sizeof(line), status_file)) {
            if (strncmp(line, "Uid:", 4) == 0) {
                sscanf(line, "Uid:\t%d", &uid);
                break;
            }
        }
        fclose(status_file);
        if (uid != target_uid) continue;

        FILE *cmdline_file = fopen(cmdline_path, "r");
        char cmdline[256] = "(unknown)";
        if (cmdline_file) {
            fgets(cmdline, sizeof(cmdline), cmdline_file);
            fclose(cmdline_file);
            if (strlen(cmdline) == 0) strcpy(cmdline, "(kernel)");
        }

        int pid = atoi(entry->d_name);
        if (kill(pid, SIGKILL) == 0) {
            log_entry(username, cmdline, "FAILED");
        }
    }

    closedir(proc);
}

void revert_user(const char *username) {
    log_entry(username, "restore", "RUNNING");
    printf("User %s access restored.\n", username);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <list|daemon|stop|fail|revert> <username>\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "list") == 0) {
        list_processes(argv[2]);
    } else if (strcmp(argv[1], "daemon") == 0) {
        run_daemon(argv[2]);
    } else if (strcmp(argv[1], "stop") == 0) {
        stop_daemon(argv[2]);
    } else if (strcmp(argv[1], "fail") == 0) {
        fail_user(argv[2]);
    } else if (strcmp(argv[1], "revert") == 0) {
        revert_user(argv[2]);
    } else {
        fprintf(stderr, "Invalid command. Use: list, daemon, stop, fail, revert\n");
        return 1;
    }

    return 0;
}
