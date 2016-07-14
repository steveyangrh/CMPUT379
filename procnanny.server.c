#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdlib.h>
//#include "memwatch.h"
//include all the files from the standard library

#define MY_PORT 1226

FILE *pFile;

FILE *log_file;

char *filename;

pid_t ppid;
//this is the pid of the parent

int num_client=0;
//num_client contains the number of client

int num_line;
//num_line indicates the number of lines in the

int rereadflag=0;
//this flag is used to check if reread a file is needed

char server_hostname[1024];
//server_hostname stores the hostname of the server

char str[300][50];
//this stores the name of the processes

char hostnames[300][40];
//hostnames stores the hostnames of the clients

int pro_time[300];
//this stores the time requirement for each specific process

void print_server_info(int a,char *b);
//used to print information to the file

void re_read(char argv[]);
//this function is used to reread the configuration file

void send_testfile(int fs);
//this is used to send message

void logscan();
//this is used to count how many processes it has killed and exit the program

void sig_handler(int sig);
//this is used to handle the signals

void printtolog(char *messgae);
//used to print message to logfile

void printreread(char *argv);
//print out the message when the file is reread

void processmonitor(int a,char *b,char *node);
//print out the message when the client is monitoring a process

void processkilled(int a,char *b,int num_time,char *node);
//print out the message when a process has been killed by the client

void processnotfound(char *b,char *node);
//print out the message when a process was not found on a client machine

void print_server_info_to_log(int a,char *b);

int snew[100];
//snew are sockets

int max_clients=100;
//100 is the number of maximun clients

int fromlength,number,outnum,sd;

int main(int argc, char const *argv[])
{
  signal(SIGINT,sig_handler);
  signal(SIGHUP,sig_handler);
  //set up the signal handler

  filename=(char *)argv[1];

  struct timeval tv;
  tv.tv_sec=0;
  tv.tv_usec=0;

  gethostname(server_hostname,1024);
  //server_hostname stores the hostname of the server

  ppid=getpid();
  print_server_info((int)ppid,server_hostname);
  //print the server imformation to the fil

  char *logfile=getenv("PROCNANNYLOGS");
  log_file=fopen(logfile,"w");
  fclose(log_file);
  //clean the logfile

  print_server_info_to_log((int)ppid,server_hostname);


  int sock;
  struct sockaddr_in master, from;
  fromlength=sizeof(from);
  //set the fromlength to the length of from
  sock=socket(AF_INET,SOCK_STREAM,0);
  master.sin_family = AF_INET;
  master.sin_addr.s_addr = INADDR_ANY;
  master.sin_port = htons (MY_PORT);

  if(sock<0)
  {
    perror("Server:Cannot open master socket");
    exit(0);
  }

  if (bind (sock, (struct sockaddr*) &master, sizeof (master)))
  {
    perror ("Server: cannot bind master socket");
    exit(1);
  }

  int i;
  for (i = 0; i < max_clients; i++)
  {
    snew[i] = 0;
  }

  listen (sock, 5);
  //sock is read to accept new connections

  re_read((char *)argv[1]);
  //read the file


  fd_set sockets;
  int max_socket;
  fd_set readfds;

  num_client=0;//initialize the number of clients

  while(1)//mean loop
  {
    FD_ZERO(&sockets);
    FD_SET(sock,&sockets);

    //max_socket=sock;

    //add child sockets to set
    /*for (i = 0 ; i < max_clients ; i++)
    {
      //socket descriptor
      sd =snew[i];
      //if valid socket descriptor then add to read list
      if(sd > 0)
        FD_SET(sd , &sockets);
      //highest file descriptor number, need it for the select function
      if(sd > max_socket)
        max_socket = sd;
    }*/

    max_socket=getdtablesize();

    tv.tv_sec=0;
    tv.tv_usec=0;
    select(max_socket,&sockets,NULL,NULL,&tv);

    if(FD_ISSET(sock,&sockets))//this part is used to accept new clients
    {
      snew[num_client] = accept (sock , (struct sockaddr*) & from,(socklen_t *) & fromlength);
      num_client=num_client+1;
      read(snew[num_client-1],hostnames[num_client-1],40);
      write(snew[num_client-1],&num_line,4);
      send_testfile(snew[num_client-1]);//this part is used to send the configuration file to the client
    }


    for(i=0;i<num_client;i++)
    {
      if(snew[i]!=0)
      {
        int checkconnection;
        FD_ZERO(&readfds);
        FD_SET(snew[i],&readfds);
        int maxdesc=getdtablesize();
        tv.tv_sec=0;
        tv.tv_usec=0;
        //printf("");
        fflush(stdout);
        int read_ready=select(maxdesc,&readfds,NULL,NULL,&tv);
        //printf("");
        //printf("read_ready is %d\n",read_ready);
        //fflush(stdout);
        while(read_ready>0)
        {
          char client_message[256];
          checkconnection=read(snew[i],client_message,256);
          if(checkconnection==0)
          {
            snew[i]=0;
            break;
          }//check if the client is still connected to the server
          printtolog(client_message);
          FD_ZERO(&readfds);
          FD_SET(snew[i],&readfds);
          tv.tv_sec=0;
          tv.tv_usec=0;
          //printf("");
          fflush(stdout);
          read_ready=select(maxdesc,&readfds,NULL,NULL,&tv);
          //printf("");
          fflush(stdout);
        }
      }
    }
  }//main while loop
  return 0;
}

void re_read(char argv[])
{
  //printchecker=0;
  num_line=-1;
  pFile = fopen(argv,"r");//open the config file
  while(!feof(pFile))
  {
    num_line++;
    fscanf(pFile,"%s %d",str[num_line],&pro_time[num_line]);
  }
  num_line++;
  //count the number of processes and put them into an array
  fclose(pFile);//close the file
  //printf("num line is %d\n",num_line);
}

void logscan()
{
  char *env=getenv("PROCNANNYLOGS");
  FILE *flog;
  int totalkilled=0;
  char readline[256];
  flog=fopen(env,"r");
  while(fgets(readline,200,flog)!=NULL)
  {
    if(strstr(readline,"killed")!=NULL)
    {
      totalkilled++;
    }
  }//used to find how my processes got killed
  fclose(flog);
  flog=fopen(env,"a");
  char info[300];
  time_t timer;
  time(&timer);
  char *time=ctime(&timer);
  time[strlen(time)-1] = '\0';
  sprintf(info,"[%s] %s %d %s",time," Info: Caught SIGINT. Exiting cleanly. ",totalkilled," process(es) killed on node ");
  int i;
  for(i=0;i<num_client;i++)
  {
    sprintf(info,"%s %s",info,hostnames[i]);
  }
  fprintf(flog,"%s\n",info);
  fflush(flog);
  printf("%s\n",info);
  fflush(stdout);
  fclose(flog);
  kill((pid_t)ppid,9);
}

void sig_handler(int sig)
{
  if(sig==SIGINT)
  {
    int i;
    for(i=0;i<num_client;i++)
    {
      if(snew[i]!=0)
      {
        int tell_clients_to_die=2;
        write(snew[i],&tell_clients_to_die,4);
      }
    }
    logscan();
    kill(ppid,9);
  }
  if(sig==SIGHUP)
  {
    int tellclients=1;
    re_read(filename);
    printreread(filename);
    int i;
    for(i=0;i<num_client;i++)
    {
      if(snew[i]!=0)
      {
        write(snew[i],&tellclients,4);
        write(snew[i],&num_line,4);
        send_testfile(snew[i]);
      }
    }
  }
}

void printreread(char *argv)
{
  char *env=getenv("PROCNANNYLOGS");
  FILE *flog;
  flog=fopen(env,"a");
  time_t timer;
  time(&timer);
  char *time=ctime(&timer);
  time[ strlen(time) - 1 ] = '\0';
  char info[300];
  sprintf(info,"[%s] %s %s' re-read",time," Info: Caught SIGHUP. Configuration file '",argv);
  fprintf(flog,"%s\n",info);
  fflush(flog);
  printf("%s\n",info);
  fflush(stdout);
  fclose(flog);
}

void send_testfile(int fs)
{
  int send_message;
  for(send_message=0;send_message<num_line;send_message++)
  {
    write(fs,&pro_time[send_message],4);
    write(fs,str[send_message],50);
  }
}

void print_server_info(int a,char *b)
{
  char *env=getenv("PROCNANNYSERVERINFO");
  FILE *flog;
  flog=fopen(env,"w");
  time_t timer;
  time(&timer);
  char *time=ctime(&timer);
  time[ strlen(time) - 1 ] = '\0';
  char info[300];
  sprintf(info,"NODE %s PID %d PORT %d",b,a,MY_PORT);
  fprintf(flog,"%s\n",info);
  fflush(flog);
  fclose(flog);
}

void print_server_info_to_log(int a,char *b)
{
  char *env=getenv("PROCNANNYLOGS");
  FILE *flog;
  flog=fopen(env,"a");
  if(flog==NULL)
  {
    perror("fail");
  }
  time_t timer;
  time(&timer);
  char *time=ctime(&timer);
  time[ strlen(time) - 1 ] = '\0';
  char info[300];
  sprintf(info,"[%s] %s %d %s %s %s %d\n",time," Procnanny server: PID ",a," on node ",b," Port ",MY_PORT);
  fprintf(flog,"%s",info);
  fflush(flog);
  fclose(flog);
}

void printtolog(char *messgae)
{
  char *env=getenv("PROCNANNYLOGS");
  FILE *flog;
  flog=fopen(env,"a");
  fprintf(flog,"%s",messgae);
  fflush(flog);
  fclose(flog);
}
