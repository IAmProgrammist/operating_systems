#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAX_LEVEL 3

int main() {
    for (int current_level = 0; current_level < MAX_LEVEL - 1; current_level++) {
        /*
        Выводим текущую глубину дерева, PID и PPID.
        */
        printf("Мы находимся на глубине %d\nТекущий pid = %d и ppid = %d\n", current_level, getpid(), getppid());

        /*
        Если это дочерний процесс, переходим к следующему шагу цикла, увеличивая глубину
        */
        pid_t left = fork();
        if (left == 0) continue;
        
        /*
        Аналогичная проверка для правого поддерева
        */
        pid_t right = fork();
        if (right == 0) continue;

        /*
        Ожидаем окончание листов/корней
        */
        int status = 0;

        waitpid(left, &status, 0);
        if (status) { 
            printf("Лист/корень завершился с ошибкой!\n");
            exit(status);
        }

        waitpid(right, &status, 0);
        if (status) { 
            printf("Лист/корень завершился с ошибкой!\n");
            exit(status);
        }

        exit(0);
    }

    printf("Мы находимся на глубине %d\nТекущий pid = %d и ppid = %d\n", MAX_LEVEL - 1, getpid(), getppid());
    sleep(3);
    exit(0); 

    return 0;
}