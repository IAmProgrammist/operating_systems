#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAX_LEVEL 3

int main() {
    for (int current_level = 0; current_level < MAX_LEVEL; current_level++) {
        printf("Мы находимся на глубине %d\n", current_level);
        printf("Текущий pid = %d и ppid = %d\n", getpid(), getppid());
        pid_t left = fork();
        if (left == 0) continue;
        
        pid_t right = fork();
        if (right == 0) continue;

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

    exit(0); 

    return 0;
}