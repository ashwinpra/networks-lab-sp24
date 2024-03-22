#include<stdio.h> 

int main(){
    FILE* fp = fopen("test3.txt", "w");

    if(fp == NULL){
        printf("File not opened\n");
        return -1;
    }

    for(int i=0; i<1000; i++){
        fprintf(fp, "User 3: %d\n", i);
    }
}