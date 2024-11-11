#ifndef SHARED
#define SHARED

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <stdbool.h>

// Лаба слишком вкусная... Пойду точить...

// Хранилище. В стандартных настройках первые 4 бита занимают тортики, 
// 4-8 биты: пироженки, 8-12: конфетки, 12-16: пряники
u_int64_t storage = 0;
// Мьютексы для потоков
pthread_mutexattr_t mutex_attr;
pthread_mutex_t mutex;

// Флаг, работает ли основной цикл
bool is_running = true;

// Максимальное количество вкусняшек на складе
#define MAX_STORAGE_SIZE 10
// Сколько битов занимает склад для одной вкусняшки
#define STORAGE_CHUNK_SIZE 4
// Максимальное количество разновидностей вкусняшек
#define MAX_CHUNKS_AMOUNT (sizeof(storage) * 8 / STORAGE_CHUNK_SIZE)
// Базовая маска для вкусняшки 
#define STORAGE_CHUNK_MASK ((1 << STORAGE_CHUNK_SIZE) - 1)

// Отступ для тортиков
#define CAKE_OFFSET (STORAGE_CHUNK_SIZE * 0)
// Отступ для пироженок
#define BROWNIE_OFFSET (STORAGE_CHUNK_SIZE * 1)
// Отступ для конфеток
#define CANDY_OFFSET (STORAGE_CHUNK_SIZE * 2)
// Отступ для пряников
#define GINGERBREAD_OFFSET (STORAGE_CHUNK_SIZE * 3)

long current_millis()
{
    struct timeval tp;

    gettimeofday(&tp, NULL);
    return tp.tv_sec * 1000 + tp.tv_usec / 1000;
}

// Устанавливает количество вкусняшек по offset в storage
void set_amount_src(u_int64_t* storage_address, int offset, u_int64_t amount)
{
    (*storage_address) = ((*storage_address) & (~(STORAGE_CHUNK_MASK << offset))) | ((amount & STORAGE_CHUNK_MASK) << offset);
}

void set_amount(int offset, u_int64_t amount)
{
    set_amount_src(&storage, offset, amount);
}

// Возвращает количество вкусняшек по offset в storage
u_int64_t get_amount_src(u_int64_t* storage_address, int offset)
{
    return (((*storage_address) & (STORAGE_CHUNK_MASK << offset)) >> offset);
}

u_int64_t get_amount(int offset)
{
    return get_amount_src(&storage, offset);
}

// Возвращает общее количество вкусняшек
u_int64_t get_storage_amount_src(u_int64_t* storage_address)
{
    return get_amount_src(storage_address, CAKE_OFFSET) + get_amount_src(storage_address, BROWNIE_OFFSET) +
           get_amount_src(storage_address, CANDY_OFFSET) + get_amount_src(storage_address, GINGERBREAD_OFFSET);
}

u_int64_t get_storage_amount() {
    return get_storage_amount_src(&storage);
}

u_int64_t cakes_thrown = 0;
u_int64_t brownies_thrown = 0;
u_int64_t candies_thrown = 0;
u_int64_t gingerbreads_thrown = 0;

u_int64_t cakes_baked = 0;
u_int64_t brownies_baked = 0;
u_int64_t candies_baked = 0;
u_int64_t gingerbreads_baked = 0;

u_int64_t cakes_ate = 0;
u_int64_t brownies_ate = 0;
u_int64_t candies_ate = 0;
u_int64_t gingerbreads_ate = 0;

void print_report() {
    printf("А теперь посчитаем...\n");
    printf("Тортики:\n");
    printf("- Испечено: %lu\n", cakes_baked);
    printf("- Выброшено: %lu\n", cakes_thrown);
    printf("- Слопано: %lu\n\n", cakes_ate);
    printf("Пироженки:\n");
    printf("- Испечено: %lu\n", brownies_baked);
    printf("- Выброшено: %lu\n", brownies_thrown);
    printf("- Слопано: %lu\n\n", brownies_ate);
    printf("Конфетки:\n");
    printf("- Испечено: %lu\n", candies_baked);
    printf("- Выброшено: %lu\n", candies_thrown);
    printf("- Слопано: %lu\n\n", candies_ate);
    printf("Пряники:\n");
    printf("- Испечено: %lu\n", gingerbreads_baked);
    printf("- Выброшено: %lu\n", gingerbreads_thrown);
    printf("- Слопано: %lu\n", gingerbreads_ate);
}

#endif