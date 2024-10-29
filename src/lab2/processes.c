#include <unistd.h>
#include <semaphore.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <fcntl.h>    /* константы O_* */
#include <sys/stat.h> /* константы для mode */
#include <semaphore.h>
#include <stdio.h>
#include <sys/mman.h>
#include <string.h>
#include <signal.h>
#include "shared.h"

#define STORAGE_SHM_KEY "os_lab_2_task_2_processes_storage"
#define SEMAPHORE_KEY "os_lab_2_task_2_processes_semaphore"

u_int64_t *shm_storage;
sem_t *shm_sem;
pid_t PIDS[7] = {};
int pids_size = 0;

void create_workshop(u_int64_t *args)
{
    pid_t workshop_pid = fork();
    if (workshop_pid == -1)
    {
        printf(stderr, "Не удалось создать цех.\n");
        exit(1);
    }

    if (workshop_pid != 0)
    {
        PIDS[pids_size++] = workshop_pid;
        return;
    }

    attach();
    while (is_running)
    {
        u_int64_t plus_amnt = args[0];
        u_int64_t plus_offs = args[1];
        usleep(args[2] * 1000);
        sem_wait(shm_sem);
        u_int64_t total = get_storage_amount_src(shm_storage);
        if (total + plus_amnt > MAX_STORAGE_SIZE)
        {
            printf("Место на складе закончилось! Выкидываем еду типа %llu в кол-ве: %llu\n", plus_offs, total + plus_amnt - MAX_STORAGE_SIZE);
            plus_amnt -= total + plus_amnt - MAX_STORAGE_SIZE;
        }

        if (plus_amnt != 0)
        {
            printf("С пылу с жару! Добавляем %llu хлебобулочных изделий. А тип продукции: %llu.\n", plus_amnt, plus_offs);
        }

        set_amount_src(shm_storage, plus_offs, get_amount_src(shm_storage, plus_offs) + plus_amnt);
        sem_post(shm_sem);
    }
    printf("Цех умер!\n");
    exit(0);
}

void create_factory(u_int64_t *data)
{
    for (int i = 0; i < data[0]; i++)
    {
        create_workshop(data + i * 3 + 1);
    }
}

bool serpent_head_rec(u_int64_t serpent_preferences, int current_offset, int max_offset)
{
    u_int64_t offset = STORAGE_CHUNK_SIZE * current_offset;
    u_int64_t mask_with_offset = STORAGE_CHUNK_MASK << offset;
    if (((*shm_storage) & mask_with_offset) && (serpent_preferences & mask_with_offset))
    {
        u_int64_t storage_amount = get_amount_src(shm_storage, offset);
        if (max_offset == -1 || max_offset != -1 && storage_amount > get_amount_src(shm_storage, max_offset))
        {
            max_offset = offset;
        }
    }

    if (current_offset != 0)
    {
        bool result = serpent_head_rec(serpent_preferences, current_offset - 1, max_offset);
        return result;
    }

    if (max_offset == -1)
    {
        // printf("Змей Горыныч очень расстроен! Он не нашёл, чего поесть. Голова ушла уничтожать деревню и скоро вернётся.\n");
        return false;
    }

    printf("Сейчас мы схаваем одну булку из %llu булок! А тип продукции: %llu.\n", get_amount_src(shm_storage, max_offset), max_offset);
    set_amount_src(shm_storage, max_offset, get_amount_src(shm_storage, max_offset) - 1);

    return true;
}

void create_serpent(u_int64_t *data)
{
    pid_t serpent_pid = fork();
    if (serpent_pid == -1)
    {
        fprintf(stderr, "Не удалось создать голову змея.\n");
        exit(1);
    }

    if (serpent_pid != 0)
    {
        PIDS[pids_size++] = serpent_pid;
        return;
    }

    attach();
    while (is_running)
    {
        sem_wait(shm_sem);
        bool result = serpent_head_rec(data[0], MAX_CHUNKS_AMOUNT - 1, -1);
        sem_post(shm_sem);
        if (result)
            usleep(data[1] * 1000);
    }
    printf("Змей Горыныч умер!\n");
    exit(0);
}

// Нам прилетел SIGUSR1, ставим флаг на "работающий" на false.
// Можно было бы убивать процесс вручную, однако так делать не стоит.
// Нужно дать процессу завершиться самому
void sigusr1()
{
    printf("Нужно прекращать работу!\n");
    is_running = false;
}

// Здесь мы присоединяемся к семафору и shared memory
void attach()
{
    signal(SIGUSR1, sigusr1);

    // storage должен существовать в обязательном порядке
    int storage_descriptor = shm_open(STORAGE_SHM_KEY,
                                      O_RDWR,
                                      0644);
    if (storage_descriptor < 0)
    {
        fprintf(stderr, "Невозможно получить доступ к складу.\n");
        exit(1);
    }
    ftruncate(storage_descriptor, sizeof(storage));

    caddr_t storage_ptr = mmap(NULL,
                               sizeof(storage),
                               PROT_READ | PROT_WRITE,
                               MAP_SHARED,
                               storage_descriptor,
                               0);
    shm_storage = (u_int64_t *)storage_ptr;

    // Семафор также должен существовать
    shm_sem = sem_open(SEMAPHORE_KEY, 0, 0644);

    if (shm_sem == SEM_FAILED)
    {
        fprintf(stderr, "Невозможно получить доступ к семафору.\n");
        exit(1);
    }
}

void init()
{
    // Запишем в SHM наш склад, чтобы дочерние процессы могли к нему обращаться
    // Создадим область в shared memory
    int fd = shm_open(STORAGE_SHM_KEY,
                      O_RDWR | O_CREAT,
                      0644);
    if (fd < 0)
    {
        fprintf(stderr, "Не удалось инициализировать склад.\n");
        exit(1);
    }

    // Установим размер нашего shared memory участка
    ftruncate(fd, sizeof(storage));

    // Теперь отобразим область в shared memory в адресное пространство программы и синхронизируем его
    caddr_t memptr = mmap(NULL,
                          sizeof(storage),
                          PROT_READ | PROT_WRITE,
                          MAP_SHARED,
                          fd,
                          0);
    if ((caddr_t)-1 == memptr)
        exit(1);

    // Теперь остаётся просто скопировать исходное значение storage в SHM
    memcpy(memptr, &storage, sizeof(storage));

    sem_t *semaphore = sem_open(SEMAPHORE_KEY, O_CREAT, 0644, 1);
}

void on_close()
{
    // Делаем анлинк склада и семафора
    shm_unlink(STORAGE_SHM_KEY);
    sem_unlink(SEMAPHORE_KEY);
}

int main()
{
    init();

    u_int64_t factory_a_args[] = {2,
                                  1, CAKE_OFFSET, 70,
                                  2, BROWNIE_OFFSET, 50};

    u_int64_t factory_b_args[] = {2,
                                  3, CANDY_OFFSET, 60,
                                  1, GINGERBREAD_OFFSET, 40};

    u_int64_t serpent_head_a_args[] = {(STORAGE_CHUNK_MASK << CAKE_OFFSET) |
                                           (STORAGE_CHUNK_MASK << CANDY_OFFSET),
                                       80};

    u_int64_t serpent_head_b_args[] = {(STORAGE_CHUNK_MASK << BROWNIE_OFFSET) |
                                           (STORAGE_CHUNK_MASK << GINGERBREAD_OFFSET),
                                       90};

    u_int64_t serpent_head_c_args[] = {(STORAGE_CHUNK_MASK << BROWNIE_OFFSET) |
                                           (STORAGE_CHUNK_MASK << GINGERBREAD_OFFSET) |
                                           (STORAGE_CHUNK_MASK << CAKE_OFFSET) |
                                           (STORAGE_CHUNK_MASK << CANDY_OFFSET),
                                       70};

    create_factory(factory_a_args);
    create_factory(factory_b_args);
    create_serpent(serpent_head_a_args);
    create_serpent(serpent_head_b_args);
    create_serpent(serpent_head_c_args);

    getchar();

    // Посылаем всем рабочим процессам сигнал SIGUSR1 и ждём, пока они закончатся
    for (int i = 0; i < pids_size; i++)
    {
        kill(PIDS[i], SIGUSR1);
        int status;
        waitpid(PIDS[i], &status, 0);
        while (!WIFEXITED(status))
        {
            waitpid(PIDS[i], &status, 0);
        }
    }

    on_close();

    return 0;
}