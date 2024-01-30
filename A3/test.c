#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<ctype.h>

void strstrip(char* s) {
    char* start = s;
    char* end = s + strlen(s);

    while (isspace((unsigned char)*start)) start++;

    if (end > start) {
        while (isspace((unsigned char)*(end - 1))) end--;
    }

    memmove(s, start, end - start);

    // Null-terminate the string
    s[end - start] = '\0';
}

int main(int argc, char const *argv[])
{
    char* line = malloc(100);
    strcpy(line, "   hello   ");
    printf("%s\n", line);
    strstrip(line);
    printf("%s\n", line);
    return 0;
}
