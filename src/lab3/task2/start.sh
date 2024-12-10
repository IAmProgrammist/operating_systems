mkdir -p tmp/lab3
chmod 777 -R ./tmp/lab3
chown $USER:$USER -R ./tmp/lab3
gcc -D_FILE_OFFSET_BITS=64 -std=c99 -Wall -Wextra -pedantic task2.c -l fuse3 -o task2.o
./task2.o './tmp/lab3'