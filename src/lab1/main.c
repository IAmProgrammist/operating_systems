#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAX_LEVEL 3

int main()
{
    int status = 0;
    printf("Мы начали в корне 0! Тут pid = %d.\n", getpid());

    pid_t node_1 = fork();
    if (node_1 == 0)
    {
        printf("Мы в поддереве 1! Тут pid = %d, ppid = %d.\n", getpid(), getppid());

        pid_t node_3 = fork();
        if (node_3 == 0)
        {
            printf("Мы в листе 3! Тут pid = %d, ppid = %d.\nКажется, этот лист собирается расти!\n", getpid(), getppid());
            pid_t node_7 = fork();
            if (node_7 == 0)
            {
                printf("Мы в свежем листе 7! Тут pid = %d, ppid = %d.\n", getpid(), getppid());
                sleep(1);
                exit(0);
            }
            pid_t node_8 = fork();
            if (node_8 == 0)
            {
                printf("Мы в свежем листе 8! Тут pid = %d, ppid = %d.\n", getpid(), getppid());
                sleep(2);
                exit(0);
            }

            printf("Ожидаем окончания листа 7 с pid = %d.\n", node_7);
            waitpid(node_7, &status, 0);
            if (status)
            {
                printf("Лист 7 завершился с ошибкой!\n");
                exit(status);
            }

            printf("Ожидаем окончания листа 8 с pid = %d.\n", node_8);
            waitpid(node_8, &status, 0);
            if (status)
            {
                printf("Лист 8 завершился с ошибкой!\n");
                exit(status);
            }

            exit(0);
        }

        pid_t node_4 = fork();

        if (node_4 == 0)
        {
            printf("Мы в листе 4! Тут pid = %d, ppid = %d.\n", getpid(), getppid());
            sleep(3);
            exit(0);
        }

        printf("Ожидаем окончания листа 3 с pid = %d.\n", node_3);
        waitpid(node_3, &status, 0);
        if (status)
        {
            printf("Лист 3 завершился с ошибкой!\n");
            exit(status);
        }

        printf("Ожидаем окончания листа 4 с pid = %d.\n", node_4);
        waitpid(node_4, &status, 0);
        if (status)
        {
            printf("Лист 4 завершился с ошибкой!\n");
            exit(status);
        }

        exit(0);
    }

    pid_t node_2 = fork();

    if (node_2 == 0)
    {
        printf("Мы в поддереве 2! Тут pid = %d, ppid = %d.\n", getpid(), getppid());

        pid_t node_5 = fork();
        if (node_5 == 0)
        {
            printf("Мы в листе 5! Тут pid = %d, ppid = %d.\n", getpid(), getppid());
            sleep(4);
            exit(0);
        }

        pid_t node_6 = fork();

        if (node_6 == 0)
        {
            printf("Мы в листе 6! Тут pid = %d, ppid = %d.\n", getpid(), getppid());
            sleep(5);
            exit(0);
        }

        printf("Ожидаем окончания листа 5 с pid = %d.\n", node_5);
        waitpid(node_5, &status, 0);
        if (status)
        {
            printf("Лист 5 завершился с ошибкой!\n");
            exit(status);
        }

        printf("Ожидаем окончания листа 6 с pid = %d.\n", node_6);
        waitpid(node_6, &status, 0);
        if (status)
        {
            printf("Лист 6 завершился с ошибкой!\n");
            exit(status);
        }

        exit(0);
    }

    printf("Ожидаем окончания поддерева 1 с pid = %d.\n", node_1);
    waitpid(node_1, &status, 0);
    if (status)
    {
        printf("Поддерево 1 завершилось с ошибкой!\n");
        exit(status);
    }

    printf("Ожидаем окончания поддерева 2 с pid = %d.\n", node_2);
    waitpid(node_2, &status, 0);
    if (status)
    {
        printf("Поддерево 2 завершилось с ошибкой!\n");
        exit(status);
    }

    return 0;
}