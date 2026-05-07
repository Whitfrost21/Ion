#include<stdio.h>
#include<stdlib.h>
#include<string.h>

char **parse(char* line){
    char **args=malloc(10*sizeof(char*));
  int i=0;
  char *tok=strtok(line," ");
  while(tok!=NULL){
    args[i++]=tok;
    tok=strtok(NULL," ");
}
args[i]=NULL;
return args;
}
int main(){ 
  while(1){
char *line = malloc(200);
  printf("\n myshell>");
  if(fgets(line,200,stdin)==NULL){
free(line);
break;
}
  
  line[strcspn(line,"\n")]='\0';
  char** args=parse(line);
  for(int i=0;args[i]!=NULL;i++){
printf("%s\n",args[i]);
}
free(args);
free(line);
}
return 0;
}
