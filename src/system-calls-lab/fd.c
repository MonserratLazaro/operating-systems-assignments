#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

/** 
 * ACTIVITY 1
 * checkValue: checks if fd, n or m are negative,
 * displays an error message and terminates the program
 */
void checkValue(int value) {
    if (value < 0) {
        if (errno == EAGAIN) {
            printf("Error: File is locked\n");
        } else if (errno == EACCES) {
            printf("Error: Permission problem accessing the file\n");
        } else if (errno == EBADF) {
            printf("Error: Bad file descriptor\n");
        } else {
            perror("Error");
        }
        exit(1);
    }
}

int main() {
    int fd, n, m;

    /** 
     * ACTIVITIES 2 AND 3
     * Write an array of floating point numbers to datos.txt,
     * then read and display them on screen
     */
    float array[4] = {1.4, 3.35, 7.99, 9};
    float array_copy[4] = {0};

    fd = creat("datos.txt", 0777);
    checkValue(fd);

    n = write(fd, array, sizeof(array));
    checkValue(n);

    close(fd);

    fd = open("datos.txt", 0);  
    checkValue(fd);

    m = read(fd, array_copy, sizeof(array_copy));
    checkValue(m);

    printf("Contents of datos.txt:\n");
    for (int i = 0; i < 4; i++) {
        printf("%.2f ", array_copy[i]);
    }
    printf("\n\n");

    close(fd);

    /** 
     * ACTIVITIES 4 AND 5
     * Write 5 floats to datos2.txt one by one,
     * printing the value of n on each write,
     * then read and display them on screen
     */
    fd = creat("datos2.txt", 0777);
    checkValue(fd);

    float v = 0.0;
    for (int i = 0; i < 5; i++) {
        v += (3.79 + i);
        n = write(fd, &v, sizeof(v));  
        checkValue(n);
        printf("Writing %.2f to datos2.txt, n = %d\n", v, n);
    }

    close(fd);

    fd = open("datos2.txt", 0);  
    checkValue(fd);

    printf("\nContents of datos2.txt:\n");
    for (int i = 0; i < 5; i++) {  
        float num;
        m = read(fd, &num, sizeof(num));
        checkValue(m);
        printf("%.2f ", num);   
    }
    printf("\n");

    close(fd);

    return 0;
}