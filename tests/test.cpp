#include <iostream>
#include <stdio.h>
#include <stdlib.h>
//#include <bits/stdc++.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <string>

using namespace std;

const unsigned long int MAX_READING_SIZE = 4096;

int main(int argc, char *argv[]){
  if(argc != 2){
    cout << "usage: " << argv[0] << " <file_name>" << endl;
    return 0;
  }
  string fileName_str = argv[1];
  fileName_str = "./"+fileName_str;
  const char *fileName_c = fileName_str.c_str();
  
  //Taking starting time
  struct timeval start_time, end_time;
  gettimeofday(&start_time, NULL);
  
  FILE *file_p = fopen(fileName_c,"r");
  if(file_p == NULL){
    cout << "Unable to open '" << fileName_c << "'" << endl;
    return 1;
  }
  fseek(file_p, 0L, SEEK_END);
  size_t fileSize = ftell(file_p);
  rewind(file_p);
  void *fileContent = malloc(MAX_READING_SIZE);
  
  FILE *file_copy_p = fopen("./copy","w");
  if(file_copy_p == NULL){
    cout << "Unable to open 'copy'" << endl;
    return 2;
  }
  
  size_t readByte = 0;
  size_t tmp;
  while(readByte < fileSize){
    tmp = fread(fileContent, 1, MAX_READING_SIZE, file_p); 
    if(tmp > 0){
      readByte += tmp;
      if(fwrite(fileContent, 1, MAX_READING_SIZE, file_copy_p) < 0){
        cout << "Error in writing on 'copy'" << endl;
        return 3;
      }
    }
  }
  
  free(fileContent);
  fclose(file_p);
  fclose(file_copy_p);
  
  //Taking end time
  gettimeofday(&end_time, NULL);
  
  cout << "File '" << fileName_c << "' copied successfully!" << endl;
  
  //Calculating elapsed time
  double execution_time = (double)(end_time.tv_usec - start_time.tv_usec) / 1000000 + (double)(end_time.tv_sec - start_time.tv_sec);
  cout << "Program executed in " << execution_time << " sec" << endl;
  
  return 0;
}

