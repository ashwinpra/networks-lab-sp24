#include<stdio.h>

int main(int argc, char const *argv[])
{
    char str[] = "bruh\0lmao";
    printf("%s", str);
    return 0;
}
