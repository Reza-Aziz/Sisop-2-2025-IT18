# Sisop-2-2025-IT18
# Soal 1
# Soal 2
1. Import library dan deklarasi direktori atau file
   <pre>
     #include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <openssl/evp.h>

#define STARTERKIT_DIR "starter_kit"
#define QUARANTINE_DIR "quarantine"
#define PID_FILE "decrypt.pid"
#define LOG_FILE "activity.log"
   </pre>
2. Function daemon dan decode base64
  <pre>
    void decrypt_daemon() {
    pid_t pid = fork();

    if (pid < 0) {
        perror("Failed to fork");
        exit(EXIT_FAILURE);
    }

    if (pid > 0) {
        printf("Daemon started with PID %d\n", pid);
        FILE *pid_file = fopen(PID_FILE, "w");
        if (!pid_file) {
            perror("Failed to write PID file");
            exit(EXIT_FAILURE);
        }
        fprintf(pid_file, "%d\n", pid);
        fclose(pid_file);

        char log_msg[100];
        snprintf(log_msg, sizeof(log_msg), "Successfully started decryption process with PID %d.", pid);
        log_activity(log_msg);

        exit(EXIT_SUCCESS);
    }

    setsid();
    chdir(".");
    fclose(stdin);
    fclose(stdout);
    fclose(stderr);

    while (1) {
        DIR *dir = opendir(STARTERKIT_DIR);  
        if (!dir) {
            sleep(5);
            continue;
        }

        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_type == DT_REG) {
                char old_path[512], new_path[512];
                snprintf(old_path, sizeof(old_path), "%s/%s", STARTERKIT_DIR, entry->d_name);

                char* decoded_name = decode_base64(entry->d_name);  
                if (decoded_name) {
                    snprintf(new_path, sizeof(new_path), "%s/decoded_%s", STARTERKIT_DIR, decoded_name);
                    rename(old_path, new_path);  
                    free(decoded_name);

                    char log_msg[600];
                    snprintf(log_msg, sizeof(log_msg), "%s - Successfully decrypted and renamed to %s", entry->d_name, decoded_name);
                    log_activity(log_msg);
                }
            }
        }

        closedir(dir);
        sleep(5);
    }
}
  </pre>
  <pre>
    char* decode_base64(const char* base64_input) {
    int length = strlen(base64_input);
    int decoded_length = (length * 3) / 4;  
    unsigned char *decoded_data = malloc(decoded_length + 1); 

    if (!decoded_data) {
        return NULL;
    }

    int actual_decoded_length = EVP_DecodeBlock(decoded_data, (const unsigned char*)base64_input, length);

    if (actual_decoded_length < 0) {
        free(decoded_data);
        return NULL;
    }

    decoded_data[actual_decoded_length] = '\0';

    return (char*)decoded_data;
}
  </pre>
3. Function qurantine dan return file untuk mobilisasi file
   * Erorr handling untuk mengecek ketersediaan direkotri yang dituju
    <pre>
       int directory_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}
   </pre>
   * Function quarantine dan return file
   <pre>
       void move_files(const char *src, const char *dest, const char *action) {
    if (!directory_exists(src)) {
        fprintf(stderr, "Source directory %s does not exist.\n", src);
        return;
    }

    DIR *dir = opendir(src);
    if (!dir) {
        perror("Failed to open source directory");
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {
            char src_path[512], dest_path[512];
            snprintf(src_path, sizeof(src_path), "%s/%s", src, entry->d_name);
            snprintf(dest_path, sizeof(dest_path), "%s/%s", dest, entry->d_name);

            if (rename(src_path, dest_path) == 0) {
                char log_msg[600];
                snprintf(log_msg, sizeof(log_msg), "%s - Successfully %s", entry->d_name, action);
                log_activity(log_msg);
            } else {
                perror("Failed to move file");
                char log_msg[600];
                snprintf(log_msg, sizeof(log_msg), "%s - Failed to %s", entry->d_name, action);
                log_activity(log_msg);
            }
        }
    }
    closedir(dir);
}
</pre>
4. Function eradicate untuk menghapus seluruh file dalam code quarantine
<pre>
  void eradicate_files() {
    if (!directory_exists(QUARANTINE_DIR)) {
        fprintf(stderr, "Quarantine directory does not exist.\n");
        return;
    }

    DIR *dir = opendir(QUARANTINE_DIR);
    if (!dir) {
        perror("Failed to open quarantine directory");
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {
            char file_path[512];
            snprintf(file_path, sizeof(file_path), "%s/%s", QUARANTINE_DIR, entry->d_name);

            if (remove(file_path) == 0) {
                char log_msg[600];
                snprintf(log_msg, sizeof(log_msg), "%s - Successfully deleted.", entry->d_name);
                log_activity(log_msg);
            } else {
                perror("Failed to delete file");
                char log_msg[600];
                snprintf(log_msg, sizeof(log_msg), "%s - Failed to delete.", entry->d_name);
                log_activity(log_msg);
            }
        }
    }

    closedir(dir);
}
  </pre>
  5. Function shutdow untuk mematikan seluruh proses daemon yang berjalan
 <pre>
   void shutdown_daemon() {
    FILE *pid_file = fopen(PID_FILE, "r");
    if (!pid_file) {
        fprintf(stderr, "PID file not found.\n");
        return;
    }

    int pid;
    if (fscanf(pid_file, "%d", &pid) != 1) {
        fprintf(stderr, "Invalid PID format.\n");
        fclose(pid_file);
        return;
    }
    fclose(pid_file);

    if (kill(pid, SIGTERM) == 0) {
        char log_msg[100];
        snprintf(log_msg, sizeof(log_msg), "Successfully shut off decryption process with PID %d.", pid);
        log_activity(log_msg);
    } else {
        perror("Failed to kill process");
        char log_msg[100];
        snprintf(log_msg, sizeof(log_msg), "Failed to shut off decryption process with PID %d.", pid);
        log_activity(log_msg);
    }
}
 </pre>
 6.  Function timestamp untuk memberikan format waktu
 <pre>
 char *timestamp() {
    static char buffer[100];
    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    strftime(buffer, sizeof(buffer), "[%d-%m-%Y][%H:%M:%S]", tm_info);
    return buffer;
}
</pre>
7. Log activity unutk mencatat segala aktivitas starterkit ke dalam file log
<pre>
     void log_activity(const char *message) {
    FILE *log = fopen(LOG_FILE, "a");
    if (!log) {
        perror("Failed to open log file");
        return;
    }
    fprintf(log, "%s - %s\n", timestamp(), message);
    fclose(log);
}
   </pre>
   8. Fungsi main untuk menerima argumen dan memproses opsi yang telah dipilih
   <pre>
     int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s --decrypt | --quarantine | --return | --eradicate | --shutdown\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (strcmp(argv[1], "--decrypt") == 0) {
        decrypt_daemon();
    } else if (strcmp(argv[1], "--quarantine") == 0) {
        move_files(STARTERKIT_DIR, QUARANTINE_DIR, "moved to quarantine directory");
    } else if (strcmp(argv[1], "--return") == 0) {
        move_files(QUARANTINE_DIR, STARTERKIT_DIR, "returned to starter kit directory");
    } else if (strcmp(argv[1], "--eradicate") == 0) {
        eradicate_files();
    } else if (strcmp(argv[1], "--shutdown") == 0) {
        shutdown_daemon();
    } else {
        fprintf(stderr, "Invalid argument.\n");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
</pre>
# Soal 3
# Soal 4
