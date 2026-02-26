#include <stdio.h>

int main(void) {
    int base, height;

    printf("Enter the base: ");
    scanf("%d", &base);
    if (base <= 0) {
        printf("Error: base must be a positive number\n");
        return 1;
    }

    printf("Enter the height: ");
    scanf("%d", &height);
    if (height <= 0) {
        printf("Error: height must be a positive number\n");
        return 1;
    }

    printf("Rectangle area: %d\n", base * height);

    return 0;
}