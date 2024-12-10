#define _POSIX_C_SOURCE 200809L // Стандарт POSIX версии 2008 года с некоторыми дополнениями
#define FUSE_USE_VERSION 30

#include <time.h>
#include <fuse3/fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h> // Для S_IFDIR и S_IFREG
#include <set>
#include <chrono>
#include <string>
#include <optional>

static const char *LOG_FILE = "/tmp/os_lab3_tmp.txt";

struct file_node {
    char*  name;
    char* contents;
    std::chrono::time_point<std::chrono::system_clock> time;
};

struct compare_file_node {
    bool operator()(const file_node &lhs, 
                    const file_node &rhs) const {
        return lhs.time < rhs.time;
    }
};


static std::multiset<file_node, compare_file_node> file_list;

void log_operation(const char *operation, const char *path) {
    FILE *fp = fopen(LOG_FILE, "a");
    if (!fp) {
        perror("Ошибка открытия лог-файла");
        return;
    }

    time_t now = time(NULL);
    struct tm *t = localtime(&now);

    fprintf(fp, "[%04d-%02d-%02d %02d:%02d:%02d] PID: %d | Операция: %s | Путь: %s\n",
            t->tm_year + 1900,
            t->tm_mon + 1,
            t->tm_mday,
            t->tm_hour,
            t->tm_min,
            t->tm_sec,
            getpid(),
            operation,
            path);

    fclose(fp);
}

void replace_file(file_node file) {
    for (auto lower = file_list.begin(); lower != file_list.end(); lower++) {
        if (strcmp(lower->name, file.name) == 0) {
            file_list.erase(lower);
            file_list.insert(file);
            return;
        }
    }

    file_list.insert(file);
}

const file_node *find_file(const char *name) {
    auto now_time = std::chrono::system_clock::now();

    for (auto lower = file_list.lower_bound({NULL, NULL, now_time}); lower != file_list.end(); lower++) {
        if (strcmp(lower->name, name) == 0) {
            return &(*lower);
        }
    }

    return NULL;
}

std::optional<const file_node> construct_file(const char* name) {
    file_node new_file;

    std::tm tm = {};
    strptime(name, "%Y-%m-%dT%H:%M:%SZ", &tm);
    int y,M,d,h,m;
    float s;
    if (sscanf(name, "%d-%d-%dT%d:%d:%fZ", &y, &M, &d, &h, &m, &s) == EOF) {
        return std::nullopt;
    }

    auto tp = std::chrono::system_clock::from_time_t(std::mktime(&tm));

    new_file.name = strdup(name);
    new_file.contents = strdup("");
    new_file.time = tp;

    return { new_file };
}

std::optional<const file_node> create_file(const char *name) {
    auto constructed_file = construct_file(name);
    if (constructed_file) {
        file_list.insert(*constructed_file);
    }

    return constructed_file;
}


int create_backup(const char *name) {
    auto file = find_file(name);
    if (!file) return -1;

    size_t bak_name_len = strlen(name) + 5; // Длина имени + ".bak"
    char *bak_name = (char *) malloc(bak_name_len);
    if (!bak_name) return -1;

    snprintf(bak_name, bak_name_len, "%s.bak", name);

    auto bak_file_opt = construct_file(bak_name);

    if (!bak_file_opt) {
        return -1;
    }

    auto bak_file = *bak_file_opt;
    free(bak_file.contents);

    bak_file.contents = strdup(file->contents);

    // Проверяем, есть ли уже .bak файл, и удаляем его
    replace_file(bak_file);

    free(bak_name);

    return 0;
}

static int fs_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    (void)fi; // Параметр не используется
    memset(stbuf, 0, sizeof(struct stat));

    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
    } else {
        auto file = find_file(path + 1);
        if (!file) {
            return -ENOENT;
        }
        stbuf->st_mode = S_IFREG | 0644;
        stbuf->st_nlink = 1;
        stbuf->st_size = strlen(file->contents);
    }

    return 0;
}

static int fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
                      struct fuse_file_info *fi, enum fuse_readdir_flags flags) {
    (void) offset;
    (void) fi;
    (void) flags; // Не используются

    if (strcmp(path, "/") != 0) {
        return -ENOENT;
    }

    filler(buf, ".", NULL, 0, (fuse_fill_dir_flags)0);
    filler(buf, "..", NULL, 0, (fuse_fill_dir_flags)0);

    for (auto ptr = file_list.begin(); ptr != file_list.end(); ptr++) {
        filler(buf, ptr->name, NULL, 0, (fuse_fill_dir_flags)0);
    }

    return 0;
}

static int fs_open(const char *path, struct fuse_file_info *fi) {
    (void) fi; // Не используются

    if (!find_file(path + 1)) {
        return -ENOENT;
    }

    return 0;
}

static int fs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    (void) fi;

    auto file = find_file(path + 1);
    if (!file) {
        return -ENOENT;
    }

    log_operation("read", path);

    size_t len = strlen(file->contents);
    if ((size_t)offset >= len) {
        return 0;
    }
    if ((size_t)offset + size > len) {
        size = len - offset;
    }
    memcpy(buf, file->contents + offset, size);
    return size;
}

static int fs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    (void) fi;

    auto file = find_file(path + 1);
    if (!file) {
        return -ENOENT;
    }

    create_backup(path + 1);

    log_operation("write", path);

    size_t len = strlen(file->contents);
    if ((size_t)offset + size > len) {
        char *new_contents = (char*)realloc(file->contents, offset + size + 1);
        if (!new_contents) {
            return -ENOMEM;
        }
        memset(new_contents + len, 0, offset + size - len);
        auto new_file_opt = construct_file(file->name);
        if (!new_file_opt) {
            return -ENOENT;
        }
        auto new_file = *new_file_opt;
        new_file.contents = new_contents;
        memcpy(new_file.contents + offset, buf, size);
        new_file.contents[offset + size] = '\0';
        return size;
    }
    memcpy(file->contents + offset, buf, size);
    file->contents[offset + size] = '\0';
    return size;
}

static int fs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    (void) mode;
    (void) fi;

    if (find_file(path + 1)) {
        return -EEXIST;
    }

    log_operation("create", path);
    if (!create_file(path + 1)) {
        return -ENOMEM;
    }
    return 0;
}

int main(int argc, char *argv[]) {
    fuse_operations fs_oper;
    fs_oper.getattr = fs_getattr;
    fs_oper.readdir = fs_readdir;
    fs_oper.open = fs_open;
    fs_oper.read = fs_read;
    fs_oper.write = fs_write;
    fs_oper.create = fs_create;

    return fuse_main(argc, argv, &fs_oper, NULL);
}