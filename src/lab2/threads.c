#include "shared.h"
#include <unistd.h>

void *workshop(void *args_outer)
{
    u_int64_t *args = (u_int64_t *)args_outer;

    while (is_running)
    {
        u_int64_t plus_amnt = args[0];
        u_int64_t plus_offs = args[1];
        // Производим товар в цехе
        usleep(args[2] * 1000);
        // Начинаем транспортировку, требующую синхронизацию
        pthread_mutex_lock(&mutex);
        u_int64_t total = get_storage_amount();
        if (total + plus_amnt > MAX_STORAGE_SIZE)
        {
            printf("Место на складе закончилось! Выкидываем еду типа %llu в кол-ве: %llu\n", plus_offs, total + plus_amnt - MAX_STORAGE_SIZE);
            plus_amnt -= total + plus_amnt - MAX_STORAGE_SIZE;
        }

        if (plus_amnt != 0)
        {
            printf("С пылу с жару! Добавляем %llu хлебобулочных изделий. А тип продукции: %llu.\n", plus_amnt, plus_offs);
        }

        set_amount(plus_offs, get_amount(plus_offs) + plus_amnt);
        pthread_mutex_unlock(&mutex);
    }

    pthread_exit(0);
}

void start_factory(u_int64_t *data)
{
    // Фабрику подразделяем на несколько цехов, которые будут заняты производством товара
    for (int i = 0; i < data[0]; i++)
    {
        pthread_t thread_workshop;
        pthread_create(&thread_workshop, NULL, workshop, (void *)(data + i * 3 + 1));
        pthread_detach(thread_workshop);
    }
}

bool serpent_head_rec(u_int64_t serpent_preferences, int current_offset, int max_offset)
{
    u_int64_t offset = STORAGE_CHUNK_SIZE * current_offset;
    u_int64_t mask_with_offset = STORAGE_CHUNK_MASK << offset;
    // Змей Горыныч будет искать предпочтительный склад с наибольшим количеством вкусняшек, чтобы
    // оптимизировать работу фабрик. Какой заботливый!
    pthread_mutex_lock(&mutex);
    if ((storage & mask_with_offset) && (serpent_preferences & mask_with_offset))
    {
        u_int64_t storage_amount = get_amount(offset);
        if (max_offset == -1 || max_offset != -1 && storage_amount > get_amount(max_offset))
        {
            max_offset = offset;
        }
    }

    if (current_offset != 0)
    {
        bool result = serpent_head_rec(serpent_preferences, current_offset - 1, max_offset);
        pthread_mutex_unlock(&mutex);
        return result;
    }

    if (max_offset == -1)
    {
        pthread_mutex_unlock(&mutex);
        return false;
    }

    printf("Сейчас мы схаваем одну булку из %llu булок! А тип продукции: %llu.\n", get_amount(max_offset), max_offset);
    set_amount(max_offset, get_amount(max_offset) - 1);

    pthread_mutex_unlock(&mutex);

    return true;
}

void *serpent_head(void *paramOuter)
{
    u_int64_t *param = (u_int64_t *)paramOuter;

    while (is_running)
    {
        bool result = serpent_head_rec(param[0], MAX_CHUNKS_AMOUNT - 1, -1);
        if (result)
            usleep(param[1] * 1000);
    }

    pthread_exit(0);
}

/*
Вариант программы, когда под ожиданием подразумевается полезная нагрузка.
Кроме того, не так оптимизирован, так как требует большего количества потоков.
Но зато можно задавать нулевые задержки.
*/
int main()
{
    pthread_t serpent_head_a, serpent_head_b, serpent_head_c,
        workshop_a, workshop_b, workshop_c, workshop_d;

    u_int64_t factory_a_args[] = {2,
                                  1, CAKE_OFFSET, 700,
                                  2, BROWNIE_OFFSET, 500};

    u_int64_t factory_b_args[] = {2,
                                  3, CANDY_OFFSET, 600,
                                  1, GINGERBREAD_OFFSET, 400};

    u_int64_t serpent_head_a_args[] = {(STORAGE_CHUNK_MASK << CAKE_OFFSET) |
                                           (STORAGE_CHUNK_MASK << CANDY_OFFSET),
                                       800};

    u_int64_t serpent_head_b_args[] = {(STORAGE_CHUNK_MASK << BROWNIE_OFFSET) |
                                           (STORAGE_CHUNK_MASK << GINGERBREAD_OFFSET),
                                       900};

    u_int64_t serpent_head_c_args[] = {(STORAGE_CHUNK_MASK << BROWNIE_OFFSET) |
                                           (STORAGE_CHUNK_MASK << GINGERBREAD_OFFSET) |
                                           (STORAGE_CHUNK_MASK << CAKE_OFFSET) |
                                           (STORAGE_CHUNK_MASK << CANDY_OFFSET),
                                       700};

    pthread_mutexattr_init(&mutex_attr);
    pthread_mutexattr_settype(&mutex_attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&mutex, &mutex_attr);

    start_factory(factory_a_args);
    start_factory(factory_b_args);
    pthread_create(&serpent_head_a, NULL, serpent_head, (void *)serpent_head_a_args);
    pthread_create(&serpent_head_b, NULL, serpent_head, (void *)serpent_head_b_args);
    pthread_create(&serpent_head_c, NULL, serpent_head, (void *)serpent_head_c_args);

    pthread_detach(serpent_head_a);
    pthread_detach(serpent_head_b);
    pthread_detach(serpent_head_c);

    pthread_mutex_destroy(&mutex);
    pthread_mutexattr_destroy(&mutex_attr);
    getchar();

    // Выходим - is_running = false. Поток должен завершиться сам.
    is_running = false;

    return 0;
}