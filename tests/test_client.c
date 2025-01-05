#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <string.h>

const unsigned long int MAX_READING_SIZE = 4096;

int main(int argc, char *argv[]){
  if(argc != 2){
    printf("usage: %s <file_name>\n", argv[0]);
    return 0;
  }
  char fileName[256];
  snprintf(fileName, sizeof(fileName), "./%s", argv[1]);
  
  //Taking starting time
  struct timeval start_time, end_time;
  gettimeofday(&start_time, NULL);
  
  FILE *file_p = fopen(fileName,"r");
  if(file_p == NULL){
    printf("Unable to open '%s'\n", fileName);
    return 1;
  }
  fseek(file_p, 0L, SEEK_END);
  size_t fileSize = ftell(file_p);
  rewind(file_p);
  void *fileContent = malloc(MAX_READING_SIZE);
  
  FILE *file_copy_p = fopen("./copy","w");
  if(file_copy_p == NULL){
    printf("Unable to open 'copy'\n");
    return 2;
  }
  
  size_t readByte = 0;
  size_t tmp;
  while(readByte < fileSize){
    tmp = fread(fileContent, 1, MAX_READING_SIZE, file_p); 
    if(tmp > 0){
      readByte += tmp;
      if(fwrite(fileContent, 1, MAX_READING_SIZE, file_copy_p) < 0){
        printf("Error in writing on 'copy'");
        return 3;
      }
    }
  }
  
  free(fileContent);
  fclose(file_p);
  fclose(file_copy_p);
  
  //Taking end time
  gettimeofday(&end_time, NULL);
  
  printf("File '%s' copied successfully!\n", fileName);
  
  //Calculating elapsed time
  double execution_time = (double)(end_time.tv_usec - start_time.tv_usec) / 1000000 + (double)(end_time.tv_sec - start_time.tv_sec);
  printf("Program executed in %lf seconds\n", execution_time);
  
  return 0;
}

