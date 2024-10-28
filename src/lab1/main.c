#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/user.h> // Для структуры user_regs_struct

#define MAX_LEVEL (3)
#define ARRAYS_AMOUNT ((1 << MAX_LEVEL) - 1)
#define ARRAY_SIZE 20
#define DELAY 400
#define FIFO_NAME "./os_lab_1_1_%d"

// Сортировка вставками, искусственная нагруженность
// создается при помощи usleep
int insertion_sort(int *array, int size)
{
    for (int i = 0; i < size; i++)
    {
        int j = i;
        while (j >= 1 && array[j] < array[j - 1])
        {
            int tmp = array[j];
            array[j] = array[j - 1];
            array[j - 1] = tmp;
            usleep(DELAY * 1000);
            j--;
        }
    }
}

void saveFifo(int id, int *array, int arraySize)
{
    char *name = malloc(sizeof(char) * 30);
    sprintf(name, FIFO_NAME, id);
    FILE *fp = fopen(name, "w");
    for (int i = 0; i < arraySize; i++)
    {
        fprintf(fp, "%d ", array[i]);
    }
    fflush(fp);
    fclose(fp);
    free(name);
}

int retreiveFifo(int id, int *array, int arraySize)
{
    char *name = malloc(sizeof(char) * 30);
    sprintf(name, FIFO_NAME, id);
    FILE *fp = fopen(name, "r");
    int element;
    int i;
    for (i = 0; i < arraySize && (fscanf(fp, "%d ", &element) != EOF); i++)
    {
        array[i] = element;
    }
    fclose(fp);
    free(name);
    return i;
}

void sortFifo(int id)
{
    pid_t currentPid = getpid();
    printf("Поддерево %d: выполняется сортировка задачи id = %d.\n", currentPid, id);
    // Создаём буффер для элементов массива
    int *buffer = malloc(sizeof(int *) * 1000);
    // Читаем числа из массива
    int bufSize = retreiveFifo(id, buffer, 1000);

    // Выполняем сортировку
    insertion_sort(buffer, bufSize);
    // Сохраняем элементы в файл
    saveFifo(id, buffer, bufSize);
    free(buffer);
}

typedef struct Delegation
{
    // На сколько уровней необходимо опустить делегацию вниз по дереву
    int level;
    // id задачи
    int id;
} Delegation;

void process_child(int pid, Delegation *delegations, int *delegationsSize)
{
    int currentPid = getpid();
    int status;
    struct user_regs_struct regs;
    if (pid != -1)
    {
        printf("Поддерево %d: ожидаем окончания поддерева с pid = %d.\n", currentPid, pid);
        // Ожидаем, пока поддерево не приостановит своё выполнение
        waitpid(pid, &status, 0);
        // Если мы не вышли из процесса
        while (!WIFEXITED(status))
        {
            // Считаем регистры
            if (ptrace(PTRACE_GETREGS, pid, NULL, &regs) == -1)
            {
                perror("ptrace GETREGS");
                exit(1);
            }

            // r14 = уровень, r15 = какая задача
            Delegation del = {regs.r14, regs.r15};

            printf("Поддерево %d: получена перенаправленная задачи на %d уровней вверх, id = %d.\n", currentPid, del.level, del.id);

            // Если необходимый уровень достиг, выполняем сортировку.

            if (del.level == 0)
                sortFifo(del.id);
            else
            {
                // Иначе - добавляем в массив передачи задач.
                // Мы передадим этот массив родителю.
                del.level--;
                delegations[(*delegationsSize)++] = del;
            }

            // Продолжить выполнение процесса до следующей остановки
            if (ptrace(PTRACE_CONT, pid, 0, 0) == -1)
            {
                perror("ptrace PTRACE_CONT");
                exit(1);
            }

            // Ожидаем следующей остановки дочернего процесса
            waitpid(pid, &status, 0);
        }

        if (status)
        {
            printf("Поддерево %d: поддерево %d завешилось с ошибкой\n", currentPid, pid);
            exit(status);
        }
    }
}

int tree_rec(int id)
{
    int delegationsSize = 0;
    Delegation *delegations = malloc(sizeof(Delegation) * (ARRAYS_AMOUNT / 2));
    // Получаем текущий pid
    int currentPid = getpid();
    printf("Поддерево %d: поддерево с id = %d начало свою работу.\n", currentPid, id);
    pid_t left = -1, right = -1;
    // Если необходимо, создаём левое поддерево
    if (id * 2 + 1 < ARRAYS_AMOUNT)
    {
        left = fork();
        if (left == 0)
            tree_rec(id * 2 + 1);

        printf("Поддерево %d: инициализация левого поддерева с pid = %d.\n", currentPid, left);
    }

    // Если необходимо, создаём правое поддерево
    if (id * 2 + 2 < ARRAYS_AMOUNT)
    {
        right = fork();
        if (right == 0)
            tree_rec(id * 2 + 2);

        printf("Поддерево %d: инициализация правого поддерева с pid = %d.\n", currentPid, right);
    }

    // Выполняем саму задачу
    sortFifo(id);

    // Ожидаем, когда свою работу закончат поддеревья. Если происходит ошибка, выходим с ошибкой
    printf("Поддерево %d: завершило свои задачи и ожидает дочерние поддеревья.\n", currentPid);

    process_child(left, delegations, &delegationsSize);
    process_child(right, delegations, &delegationsSize);

    if (ptrace(PTRACE_TRACEME, 0, NULL, NULL) == -1)
    {
        // Объявление, что текущий процесс желает быть
        // отслеживаемым родительским процессом
        perror("PTRACE_TRACEME");
        exit(1);
    }

    // Для каждой делегации из массива делегаций
    for (int i = 0; i < delegationsSize; i++)
    {
        Delegation del = delegations[i];
        int64_t delLevel = del.level;
        int64_t delId = del.id;
        printf("Поддерево %d: выполняется перенаправление задачи на %d уровней вверх, id = %d.\n", currentPid, del.level, del.id);
        // Сохраним делегацию в регистры
        __asm__ volatile(
            "movq %0, %%r14\n"
            "movq %1, %%r15\n"
            :
            : "r"(delLevel), "r"(delId)
            : "r15", "r14");
        // Выкинем статус из процесса, означающий, что в
        // нём что-то произошло. В данном случае -
        // делегация.
        raise(SIGCHLD);
        printf("Поддерево %d: задача перенаправлена.\n", currentPid);
    }

    exit(0);
}

int tree()
{
    // Создаём корень
    pid_t root = fork();
    if (root == 0)
    {
        // Запускаем рекурентную сортировку
        tree_rec(0);
        exit(0);
    }

    int status;
    waitpid(root, &status, 0);
    if (status)
    {
        exit(status);
    }
}

int main()
{
    // Наша задача - отсортировать массив чисел. Инициалазируем его
    // и заполняем случайными числами под количетсво поддеревьев и листьев
    int **sortArray = malloc(sizeof(int *) * ARRAYS_AMOUNT);
    for (int i = 0; i < ARRAYS_AMOUNT; i++)
    {
        sortArray[i] = malloc(sizeof(int) * ARRAY_SIZE);
        for (int j = 0; j < ARRAY_SIZE; j++)
        {
            sortArray[i][j] = rand() % 1000;
        }
    }

    // Запишем в файлы массивы для сортировки
    for (int i = 0; i < ARRAYS_AMOUNT; i++)
    {
        saveFifo(i, sortArray[i], ARRAY_SIZE);
    }

    printf("********************\n");
    printf("\nМассивы:\n");
    for (int i = 0; i < ARRAYS_AMOUNT; i++)
    {
        printf("%d: ", i);
        for (int j = 0; j < ARRAY_SIZE; j++)
        {
            printf("%d ", sortArray[i][j]);
        }
        printf("\n");
    }
    printf("********************\n");

    printf("Выполняется сортировка...\n");
    // Запускаем сортировку
    tree();
    printf("Сортировка выполнена\n");
    printf("********************\n");
    printf("Массивы:\n");
    for (int i = 0; i < ARRAYS_AMOUNT; i++)
    {
        printf("%d: ", i);
        int bufSize = retreiveFifo(i, sortArray[i], ARRAY_SIZE);
        for (int j = 0; j < ARRAY_SIZE; j++)
        {
            printf("%d ", sortArray[i][j]);
        }
        printf("\n");
    }
    printf("********************\n");
    return 0;
}