#include <stdio.h>
#include <time.h>

int main(){
    time_t timestamp;
    // time(&timestamp);
    sleep(2);
    time_t timestamp2;
    time(&timestamp2);
    printf("Timestamp: %s\n", ctime(&timestamp));
    printf("Timestamp2: %s\n", ctime(&timestamp2));
    printf("Difference: %f\n", difftime(timestamp2, timestamp));
}