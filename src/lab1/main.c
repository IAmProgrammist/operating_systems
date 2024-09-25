#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>

#define MAX_LEVEL 3
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

int tree_rec(int id, int *workersId, int workersSize)
{
    // Получаем текущий pid
    int currentPid = getpid();
    printf("Поддерево %d: поддерево с id = %d начало свою работу.\n", currentPid, id);
    pid_t left = -1, right = -1;
    // Если необходимо, создаём левое поддерево
    if (id * 2 + 1 < workersSize)
    {
        left = fork();
        if (left == 0)
            tree_rec(id * 2 + 1, workersId, workersSize);

        printf("Поддерево %d: инициализация левого поддерева с pid = %d.\n", currentPid, left);
    }

    // Если необходимо, создаём правое поддерево
    if (id * 2 + 2 < workersSize)
    {
        right = fork();
        if (right == 0)
            tree_rec(id * 2 + 2, workersId, workersSize);

        printf("Поддерево %d: инициализация правого поддерева с pid = %d.\n", currentPid, right);
    }

    for (int i = 0; i < workersSize; i++)
    {
        // Создаём буффер для элементов массива
        int *buffer = malloc(sizeof(int *) * 1000);
        if (id == workersId[i])
        {
            // Читаем числа из массива
            printf("Поддерево %d: обнаружено задание с id = %d.\n", currentPid, i + 1);
            int bufSize = retreiveFifo(i + 1, buffer, 1000);

            // Выполняем сортировку
            insertion_sort(buffer, bufSize);
            // Сохраняем элементы в файл
            saveFifo(i + 1, buffer, bufSize);
        }
        free(buffer);
    }

    // Ожидаем, когда свою работу закончат поддеревья. Если происходит ошибка, выходим с ошибкой
    printf("Поддерево %d: завершило свои задачи и ожидает дочерние поддеревья.\n", currentPid);
    int status;
    if (left != -1)
    {
        printf("Поддерево %d: ожидаем окончания поддерева с pid = %d.\n", currentPid, left);
        waitpid(left, &status, 0);
        if (status)
        {
            printf("Поддерево %d: поддерево %d завешилось с ошибкой\n", currentPid, left);
            exit(status);
        }
    }

    if (right != -1)
    {
        printf("Поддерево %d: ожидаем окончания поддерева с pid = %d.\n", currentPid, right);
        waitpid(right, &status, 0);
        if (status)
        {
            printf("Поддерево %d: поддерево %d завешилось с ошибкой\n", currentPid, left);
            exit(status);
        }
    }

    exit(0);
}

int tree()
{
    // Создаём корень
    pid_t root = fork();
    if (root == 0)
    {
        // Прочитываем файл, который указывает, какому 
        // элементу дерева какой массив сортировать
        int *workers = malloc(sizeof(int *) * 1000);
        int workersSize = retreiveFifo(0, workers, 1000);

        // Запускаем рекурентную сортировку
        tree_rec(0, workers, workersSize);

        // Освобождение ресурсов
        free(workers);
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
    int arraysAmount = ((1 << MAX_LEVEL) - 1);
    int **sortArray = malloc(sizeof(int *) * arraysAmount);
    for (int i = 0; i < arraysAmount; i++)
    {
        sortArray[i] = malloc(sizeof(int) * ARRAY_SIZE);
        for (int j = 0; j < ARRAY_SIZE; j++)
        {
            sortArray[i][j] = rand() % 1000;
        }
    }

    // Задаём соответствие, какой id поддерева/листа какой массив сортирует
    int *workersId = malloc(sizeof(int) * arraysAmount);
    for (int i = 0; i < arraysAmount; i++)
    {
        workersId[i] = i;
    }

    // Запишем, какому дереву соответствует какой массив в файл
    saveFifo(0, workersId, arraysAmount);
    // Запишем в файлы массивы для сортировки
    for (int i = 0; i < arraysAmount; i++)
    {
        saveFifo(i + 1, sortArray[i], ARRAY_SIZE);
    }

    printf("********************\n");
    printf("Пейлоад задач:\n");
    for (int i = 0; i < arraysAmount; i++) {
        printf("%d ", workersId[i]);
    }
    printf("\nМассивы:\n");
    for (int i = 0; i < arraysAmount; i++) {
        printf("%d: ", i);
        for (int j = 0; j < ARRAY_SIZE; j++) {
            printf("%d ", sortArray[i][j]);
        }
        printf("\n");
    }
    printf("********************\n");


    printf("Выполняется сортировка...\n");
    // Запускаем сортировку
    tree(sortArray, workersId);
    printf("Сортировка выполнена\n");
    printf("********************\n");
    printf("Массивы:\n");
    for (int i = 0; i < arraysAmount; i++) {
        printf("%d: ", i);
        int bufSize = retreiveFifo(i + 1, sortArray[i], ARRAY_SIZE);
        for (int j = 0; j < ARRAY_SIZE; j++) {
            printf("%d ", sortArray[i][j]);
        }
        printf("\n");
    }
    printf("********************\n");
    return 0;
}