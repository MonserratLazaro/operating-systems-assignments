#include <stdio.h>

int factorial(int n) {
    int result = 1;
    for (int i = 2; i <= n; i++)
        result *= i;
    return result;
}

int main(void) {
    int num;

    printf("Enter a number: ");
    scanf("%d", &num);
    if (num < 0) {
        printf("Error: number must be non-negative\n");
        return 1;
    }

    printf("Factorial of %d is %d\n", num, factorial(num));

    return 0;
}