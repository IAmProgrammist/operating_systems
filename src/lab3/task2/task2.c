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
#include <time.h>

static const char *LOG_FILE = "/tmp/os_lab3_tmp.txt";

// Возвращает 1 и сохраняет дату в parsed_time,
// если имя файла name начинается с валидной даты, иначе - 0
int parse_time(struct tm *parsed_time, const char *name)
{
    struct tm local;
    if (!strptime(name, "%d.%m.%Y %H:%M:%S", &local))
    {
        return 0;
    }

    *parsed_time = local;
    return 1;
}

// Возвращает 1, если tm1 > tm2, иначе - 0.
int cmp_time(struct tm *tm1, struct tm *tm2)
{
    if (tm1->tm_year != tm2->tm_year)
    {
        return tm1->tm_year > tm2->tm_year;
    }
    else if (tm1->tm_mon != tm2->tm_mon)
    {
        return tm1->tm_mon > tm2->tm_mon;
    }
    else if (tm1->tm_mday != tm2->tm_mday)
    {
        return tm1->tm_mday > tm2->tm_mday;
    }
    else if (tm1->tm_hour != tm2->tm_hour)
    {
        return tm1->tm_hour > tm2->tm_hour;
    }
    else if (tm1->tm_min != tm2->tm_min)
    {
        return tm1->tm_min > tm2->tm_min;
    }
    else if (tm1->tm_sec != tm2->tm_sec)
    {
        return tm1->tm_sec > tm2->tm_sec;
    }
    return 1;
}

typedef struct file_node
{
    char *name;
    char *contents;
    struct file_node *next;
    struct tm expires;
} file_node;

static file_node *file_list = NULL;

void log_operation(const char *operation, const char *path)
{
    FILE *fp = fopen(LOG_FILE, "a");
    if (!fp)
    {
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

file_node *find_file(const char *name)
{
    if (file_list == NULL)
        return NULL;

    time_t now_time_t;
    struct tm *now;
    time(&now_time_t);
    now = localtime(&now_time_t);

    while (file_list && cmp_time(now, &file_list->expires))
    {
        file_node *old_file_list = file_list;
        file_list = file_list->next;
        free(old_file_list);
    }
    file_node *current = file_list;
    while (current)
    {
        if (strcmp(current->name, name) == 0)
        {
            return current;
        }

        while (current->next && cmp_time(now, &current->next->expires))
        {
            file_node *old_next = current->next;
            current->next = current->next->next;
            free(old_next);
        }

        current = current->next;
    }
    return NULL;
}

file_node *create_file(const char *name)
{
    struct tm file_expires;
    if (!parse_time(&file_expires, name))
        return NULL;

    time_t now_time_t;
    struct tm *now;
    time(&now_time_t);
    now = localtime(&now_time_t);
    if (cmp_time(now, &file_expires))
    {
        return NULL;
    }

    file_node *new_file = malloc(sizeof(file_node));
    if (!new_file)
    {
        return NULL;
    }
    new_file->name = strdup(name);
    new_file->contents = strdup("");
    new_file->next = file_list;
    new_file->expires = file_expires;
    file_list = new_file;
    return new_file;
}

void create_backup(const char *name)
{
    file_node *file = find_file(name);
    if (!file)
        return;

    size_t bak_name_len = strlen(name) + 5; // Длина имени + ".bak"
    char *bak_name = malloc(bak_name_len);
    if (!bak_name)
        return;

    snprintf(bak_name, bak_name_len, "%s.bak", name);

    // Проверяем, есть ли уже .bak файл, и удаляем его
    file_node *bak_file = find_file(bak_name);
    if (bak_file)
    {
        free(bak_file->contents);
        bak_file->contents = NULL;
    }
    else
    {
        bak_file = create_file(bak_name);
    }

    if (bak_file)
    {
        bak_file->contents = strdup(file->contents);
    }

    free(bak_name);
}

static int fs_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi)
{
    (void)fi; // Параметр не используется
    memset(stbuf, 0, sizeof(struct stat));

    if (strcmp(path, "/") == 0)
    {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
    }
    else
    {
        file_node *file = find_file(path + 1);
        if (!file)
        {
            return -ENOENT;
        }
        stbuf->st_mode = S_IFREG | 0644;
        stbuf->st_nlink = 1;
        stbuf->st_size = strlen(file->contents);
    }

    return 0;
}

static int fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
                      struct fuse_file_info *fi, enum fuse_readdir_flags flags)
{
    (void)offset;
    (void)fi;
    (void)flags; // Не используются

    if (strcmp(path, "/") != 0)
    {
        return -ENOENT;
    }

    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);

    if (file_list == NULL)
        return 0;

    time_t now_time_t;
    struct tm *now;
    time(&now_time_t);
    now = localtime(&now_time_t);

    while (file_list && cmp_time(now, &file_list->expires))
    {
        file_node *old_file_list = file_list;
        file_list = file_list->next;
        free(old_file_list);
    }
    file_node *current = file_list;

    while (current)
    {
        filler(buf, current->name, NULL, 0, 0);

        while (current->next && cmp_time(now, &current->next->expires))
        {
            file_node *old_next = current->next;
            current->next = current->next->next;
            free(old_next);
        }

        current = current->next;
    }

    return 0;
}

static int fs_open(const char *path, struct fuse_file_info *fi)
{
    (void)fi; // Не используются

    if (!find_file(path + 1))
    {
        return -ENOENT;
    }

    return 0;
}

static int fs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    (void)fi;

    file_node *file = find_file(path + 1);
    if (!file)
    {
        return -ENOENT;
    }

    log_operation("read", path);

    size_t len = strlen(file->contents);
    if ((size_t)offset >= len)
    {
        return 0;
    }
    if ((size_t)offset + size > len)
    {
        size = len - offset;
    }
    memcpy(buf, file->contents + offset, size);
    return size;
}

static int fs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    (void)fi;

    file_node *file = find_file(path + 1);
    if (!file)
    {
        return -ENOENT;
    }

    create_backup(path + 1);

    log_operation("write", path);

    size_t len = strlen(file->contents);
    if ((size_t)offset + size > len)
    {
        char *new_contents = realloc(file->contents, offset + size + 1);
        if (!new_contents)
        {
            return -ENOMEM;
        }
        file->contents = new_contents;
        memset(file->contents + len, 0, offset + size - len);
    }
    memcpy(file->contents + offset, buf, size);
    file->contents[offset + size] = '\0';
    return size;
}

static int fs_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    (void)mode;
    (void)fi;

    if (find_file(path + 1))
    {
        return -EEXIST;
    }

    log_operation("create", path);
    if (!create_file(path + 1))
    {
        return -ENOMEM;
    }
    return 0;
}

static int fs_unlink(const char *path) {
    char* name = path + 1;
    if (file_list == NULL)
        return -ENOENT;

    time_t now_time_t;
    struct tm *now;
    time(&now_time_t);
    now = localtime(&now_time_t);

    while (file_list && cmp_time(now, &file_list->expires))
    {
        file_node *old_file_list = file_list;
        file_list = file_list->next;
        free(old_file_list);
    }

    if (strcmp(file_list->name, name) == 0)
    {
        file_node *old_file_list = file_list;
        file_list = file_list->next;
        free(old_file_list);
        return 0;
    }

    file_node *current = file_list;

    while (current)
    {
        file_node* next = current->next;

        if (!next)
            return -ENOENT;

        if (strcmp(next->name, name) == 0)
        {
            file_node *old_file = next;
            current->next = next->next;
            free(old_file);
            return 0;
        }

        while (current->next && cmp_time(now, &current->next->expires))
        {
            file_node *old_next = current->next;
            current->next = current->next->next;
            free(old_next);
        }

        current = current->next;
    }
    return -ENOENT;
}

static struct fuse_operations fs_oper = {
    .getattr = fs_getattr,
    .readdir = fs_readdir,
    .open = fs_open,
    .read = fs_read,
    .write = fs_write,
    .create = fs_create,
    .unlink = fs_unlink
};

int main(int argc, char *argv[])
{
    return fuse_main(argc, argv, &fs_oper, NULL);
}