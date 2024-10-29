#include "shared.h"

/* Контроль переходит потоковой функции */
void *factory(void *paramOuter)
{
    u_int64_t *param = (u_int64_t *)paramOuter;
    u_int64_t prod_amount = param[0];
    long init = current_millis();
    long last_check = init;

    while (is_running)
    {
        long new_check = current_millis();

        for (u_int64_t i = 0; i < prod_amount; i++)
        {
            u_int64_t plus_amnt = param[3 * i + 1];
            u_int64_t plus_offs = param[3 * i + 2];
            u_int64_t plus_time = param[3 * i + 3];
            u_int64_t total_ticks = (new_check - init) / plus_time;
            u_int64_t last_ticks = (last_check - init) / plus_time;
            u_int64_t amount_after = total_ticks - last_ticks;
            plus_amnt *= amount_after;
            // Количество продукции увеличивается в соответствие с 
            // количеством "тиков". 
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

        last_check = new_check;
    }

    pthread_exit(0);
}

void serpent_head_rec(u_int64_t serpent_preferences, int left)
{
    // Здесь обход по складу несколько раз обоснован тем, что 
    // может пройти несколько тиков. Рекурсивно мы их и обрабатываем
    if (left == 0)
        return;
    pthread_mutex_lock(&mutex);
    int max_offset = -1;
    u_int64_t max_amount = 0;
    for (int i = 0; i < MAX_CHUNKS_AMOUNT; i++)
    {
        u_int64_t offset = STORAGE_CHUNK_SIZE * i;
        u_int64_t mask_with_offset = STORAGE_CHUNK_MASK << offset;
        if ((storage & mask_with_offset) && (serpent_preferences & mask_with_offset))
        {
            u_int64_t storage_amount = get_amount(offset);
            if (max_offset == -1 || max_offset != -1 && storage_amount > max_amount)
            {
                max_amount = storage_amount;
                max_offset = offset;
            }
        }
    }
    // Грустная голова поняла, что ей здесь пока делать нечего,
    // и ушла, освободив ресурсы другим головам
    if (max_offset == -1)
    {
        pthread_mutex_unlock(&mutex);
        return;
    }

    printf("Сейчас мы схаваем одну булку из %llu булок! А тип продукции: %llu.\n", max_amount, max_offset);
    set_amount(max_offset, max_amount - 1);
    serpent_head_rec(serpent_preferences, left - 1);
    pthread_mutex_unlock(&mutex);
}

void *serpent_head(void *paramOuter)
{
    u_int64_t *param = (u_int64_t *)paramOuter;
    u_int64_t prod_amount = param[0];
    long init = current_millis();
    long last_check = init;

    while (is_running)
    {
        long new_check = current_millis();
        u_int64_t total_ticks = (new_check - init) / param[1];
        u_int64_t last_ticks = (last_check - init) / param[1];
        u_int64_t amount_after = total_ticks - last_ticks;
        // Здесь также определяем кол-во тиков

        serpent_head_rec(param[0], amount_after);

        last_check = new_check;
    }

    pthread_exit(0);
}

/*
Данный вариант не совсем корректен.
Под потреблением фабрик и змеем горынычем 
могут подразумеваться полезные нагрузки, 
однако здесь мы моделируем работу
этой системы.

Эта программа тоже имеет право на жизнь,
например если мы создаём игру про фабрики и драконов, 
здесь гораздо важнее справедливость модели и 
следование установленным правилам,  
именно поэтому мы высчитываем, сколько "тиков" прошло и 
следовательно сколько было выполнено работ.
*/
int main()
{
    pthread_t factory_a, factory_b,
        serpent_head_a, serpent_head_b, serpent_head_c;

    u_int64_t factory_a_args[] = {2,
                                  1, CAKE_OFFSET, 70,
                                  2, BROWNIE_OFFSET, 50};

    u_int64_t factory_b_args[] = {2,
                                  3, CANDY_OFFSET, 40,
                                  1, GINGERBREAD_OFFSET, 60};

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

    pthread_mutexattr_init(&mutex_attr);
    pthread_mutexattr_settype(&mutex_attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&mutex, &mutex_attr);

    pthread_create(&factory_a, NULL, factory, (void *)factory_a_args);
    pthread_create(&factory_b, NULL, factory, (void *)factory_b_args);

    pthread_create(&serpent_head_a, NULL, serpent_head, (void *)serpent_head_a_args);
    pthread_create(&serpent_head_b, NULL, serpent_head, (void *)serpent_head_b_args);
    pthread_create(&serpent_head_c, NULL, serpent_head, (void *)serpent_head_c_args);

    pthread_join(factory_a, NULL);
    pthread_join(factory_b, NULL);
    pthread_join(serpent_head_a, NULL);
    pthread_join(serpent_head_b, NULL);
    pthread_join(serpent_head_c, NULL);

    pthread_mutex_destroy(&mutex);
    pthread_mutexattr_destroy(&mutex_attr);
    getchar();

    // Выходим - is_running = false. Поток должен завершиться сам.
    is_running = false;

    return 0;
}