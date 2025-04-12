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
