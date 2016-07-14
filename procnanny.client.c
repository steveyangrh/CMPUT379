#include <sys/types.h>
#include <sys/param.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
//#include "memwatch.h"

int  fromserver, number, res;

FILE *proFile;
//FILE *bash_co;//bash_co contents the comment line

FILE *pFile;
//test.txt

FILE *commandReturn;
//this points to the return values of the command

char command[80]="";//this contains the command sent to the terminal

int checkprocessfound[400];//this is used to see if a process was found

int rereadflag=0;//this flag is used to check if reread a file is needed

pid_t children_pid[500];
//children_pid stores children pids

int monitoredpids[800];
//monitored pids stores the pids that were monitored by the children

int num_monitoredpids=0;
//this stores the number of pids that has been monitored by the children

int pidFinder(int *pidlist,FILE *commandReturn);
//used to find pids

int processkiller(int pid,int num_time);

int nothing;

int printchecker;

int num_line;
//this stores the number of lines

int hostname_size;
//hostname_size stores the size of the client hostname

char hostname[40];
//hostname stores the host name of the client

char str[300][50];
//this stores the name of the processes

int pro_time[300];
//this stores the time requirement for each specific process

int pipe_from_parent[1000][2];
//used to send messages from parents to children

int pipe_from_child[1000][2];
//used to send messages from children to parent

int numchildren;
//numchildren indicates the number of total children

int totalkilled;
//this indicates the number of processes that have been killed

int childstate;
//childstate indicates the state of a child process,0 means it's monitoring a process,1 means it's free

int isvalueinarray(int val, int *arr, int size);
//used to see if a specific value is in the array

void receive_testfile(int fs);
//this is used to send message

void processmonitor(int a,char *b);
//tell the server the client is currently monitoring a process;

void processnotfound(char *b);
//tell the server the process is not found

void processkilled(int a,char *b,int num_time);
//tell the server a process has been killed

void sig_handler(int sig);
//signal handler

void logscan();
//this is used to scan logfile to calculate how many process got killed

void re_read();


pid_t ppid;

int main(int argc, char const *argv[])
{
  signal(SIGHUP,sig_handler);
  signal(SIGINT,sig_handler);
  //this part is resevred to receive signals

  uint16_t MY_PORT=(uint16_t)atoi(argv[2]);

  fd_set writefds;

  ppid=getpid();

  gethostname(hostname,40);

  struct timeval tv;
  tv.tv_sec=5;
  tv.tv_usec=0;
  //set the timeout

  struct  sockaddr_in     server;

  struct  hostent         *host;

  host=gethostbyname(argv[1]);

  if (host == NULL)
  {
    perror ("Client: cannot get host description");
    exit (1);
  }

  fromserver= socket (AF_INET, SOCK_STREAM, 0);


  if (fromserver< 0)
  {
    perror ("Client: cannot open socket");
    exit (1);
  }

  bzero (&server, sizeof (server));
  bcopy (host->h_addr, & (server.sin_addr), host->h_length);

  server.sin_family = AF_INET;
  server.sin_port = htons (MY_PORT);
  //res = inet_pton(AF_INET, argv[1], &server.sin_addr);

  connect (fromserver, (struct sockaddr*) & server, sizeof (server));

  //write(fromserver,&hostname_size,4);
  write(fromserver,hostname,40);//send the node message to the server

  read(fromserver,&num_line,4);
  receive_testfile(fromserver);//get the killing command from the server

  numchildren=0;//at the begining, set the number of children to 0
  char ps[50]="pgrep  ";
  printchecker=num_line;

  int totalprocess=0;

  int num_per_process;

  int pidlist[300];//pidlist contains the pids

  numchildren=0;//initiate the number of children to 0

  pid_t pid;

  int i_1;
  for(i_1=0;i_1<num_line;i_1++)
  {
    sprintf(command,"%s %s",command,ps);
    sprintf(command,"%s %s",command,str[i_1]);
    //now command string consists of the unix command

    commandReturn=popen(command,"r");//commandReturn points to the command return file

    num_per_process=pidFinder(pidlist,commandReturn);//pidlist contains an array of pids

    command[0]=0;

    pclose(commandReturn);//close the comman return file

    totalprocess=totalprocess+num_per_process;

    int i_2;
    if(num_per_process!=0)
    {
      for(i_2=0;i_2<num_per_process;i_2++)
      {
        numchildren++;//increase the number of children by one

        monitoredpids[num_monitoredpids]=pidlist[i_2];//apend the pid to the end of monitoredpids, which means this pid is nonitored
        num_monitoredpids++;//increase the number of monitoredpids by one

        pipe(pipe_from_child[numchildren-1]);//create pipes from child to parent
        pipe(pipe_from_parent[numchildren-1]);//create pipes for sending data from parent to children

        if((pid=fork())<0)//fork error
        {
          printf("fork error");
          break;
        }

        else if(pid==0)  //child
        {
          childstate=0;//the child is monitoring a process currently
          int pipeindex=numchildren-1;
          write(pipe_from_child[pipeindex][1],&childstate,4);//write four bytes to the pipe,indicates the child is being used currently
          close(pipe_from_child[pipeindex][0]);//close the read end of the pipe
          close(pipe_from_parent[pipeindex][1]);//close the write end of the pipe,this pipe is used to read message from the parent

          int processid;
          processid=pidlist[i_2];
          char *processname;
          processname=str[i_1];
          int processtime;
          processtime=pro_time[i_1];

          processmonitor(processid,processname);
          int killstatus;//0 means the process is killed, 1 means no process was killed
          killstatus=processkiller(processid,processtime);

          //successfully killed a process
          if(killstatus==0)
          {
            processkilled(processid,processname,processtime);
            //print out the time when the process was killed
          }

          childstate=1;//the child is done monitoring

          write(pipe_from_child[pipeindex][1],&childstate,4);//write another 4 bytes to the pipe, indicates that the child is free to use

          while(1)
          {
            int messagesize=0;
            read(pipe_from_parent[pipeindex][0],&messagesize,4);//this is used to received the size of the message
            read(pipe_from_parent[pipeindex][0],processname,messagesize);//this is used to receive the message
            read(pipe_from_parent[pipeindex][0],&processid,4);//this is used to receive the process id
            read(pipe_from_parent[pipeindex][0],&processtime,4);//this is used to received the processtime
            write(pipe_from_child[pipeindex][1],&childstate,4);//write four bytes to the pipe,indicates the child is being used currently
            processmonitor(processid,processname);//start monitoring the process
            killstatus=processkiller(processid,processtime);//kill the process
            //successfully killed a process
            if(killstatus==0)
            {
              processkilled(processid,processname,processtime);
              //print out the time when the process was killed
            }
            childstate=1;//the child is done monitoring
            write(pipe_from_child[pipeindex][1],&childstate,4);//write another 4 bytes to the pipe, indicates that the child is free to use
          }//while in the while loop
        }//watch out,you are getting out of a child process

        children_pid[numchildren-1]=(int)pid;

      }//for(i_2=0;i_2<num_per_process;i_2++)
    }
    else
    {
      processnotfound(str[i_1]);
    }
  }//for(i_1=0;i_1<num_line;i_1++)


  for(;;)//oh my god, it's an infinite loop
  {
    FD_ZERO(&writefds);
    FD_SET(fromserver,&writefds);
    int maxdesc=getdtablesize();
    int read_read_status;
    tv.tv_sec=5;
    tv.tv_usec=0;
    read_read_status=select(maxdesc,&writefds,NULL,NULL,&tv);
    if(read_read_status>0)
    {
      int whattodo;
      read(fromserver,&whattodo,4);
      if(whattodo==1)
      {
        printchecker=0;
        read(fromserver,&num_line,4);
        receive_testfile(fromserver);//get the killing command from the server
      }
      if(whattodo==2)
      {
        raise(SIGINT);
      }
    }

    double pipechecker;
    int nbytes;

    int freechildfound=0;//when freechildfound=0,it means no free child is found, create a new one;
                         //when freechildfound=1,it means a free child is found, no need to create a new one
    int pipeindex2;

    int i_4;
    for(i_4=0;i_4<num_line;i_4++)
    {
      if(checkprocessfound[i_4]!=10)//this step is to make sure all the process can get monitored
      {
        sprintf(command,"%s %s",command,ps);
        sprintf(command,"%s %s",command,str[i_4]);
        commandReturn=popen(command,"r");//commandReturn points to the command return file
        num_per_process=pidFinder(pidlist,commandReturn);//pidlist contains an array of pids
        command[0]=0;
        pclose(commandReturn);//close the comman return file
        if(num_per_process!=0)
        {
          checkprocessfound[i_4]=1;
          int i_5;
          for(i_5=0;i_5<num_per_process;i_5++)
          {
            if(!isvalueinarray(pidlist[i_5],monitoredpids,800))
            {
              monitoredpids[num_monitoredpids]=pidlist[i_5];
              num_monitoredpids++;

              int i_6;
              for(i_6=0;i_6<numchildren;i_6++)//this step is used to find out if any children is not in use, if not, use it,if all in use, create a new one
              {
                pipeindex2=i_6;//pipeindex2 is used to check which child is free to use
                nbytes=read(pipe_from_child[i_6][0],&pipechecker,8);//try to read 8 bytes from the pipe, if 8 bytes is read, break it
                if(nbytes==8)
                {
                  int size2;
                  size2=strlen(str[i_4])+1;
                  write(pipe_from_parent[pipeindex2][1],&size2,4);//send the size of the message to the pipe
                  write(pipe_from_parent[pipeindex2][1],str[i_4],size2);//send the message to the pipe
                  write(pipe_from_parent[pipeindex2][1],&pidlist[i_5],4);//write pid to the child
                  write(pipe_from_parent[pipeindex2][1],&pro_time[i_4],4);//write process time to the child
                  freechildfound=1;
                  break;//once a free child is found, break the loop, break the inner loop
                }
                write(pipe_from_child[i_6][1],&i_6,4);//if it's 4,write 4 bytes back
              }
              if(freechildfound==1)//this means the message has already been sent to a free child, no need to do more shit
              {
                freechildfound=0;//set the variable freechildfound back to 0
                goto LABLE1;
              }

              numchildren++;//since no free child is found,increase the number of children by one and create pipes between the child and the parent
              pipe(pipe_from_child[numchildren-1]);//create pipes from child to parent
              pipe(pipe_from_parent[numchildren-1]);//create pipes for sending data from parent to children
              //you can close some pipe end here
              pid_t pid2;
              if((pid2=fork())<0)//fork error
              {
                printf("fork error");
                break;
              }
              else if(pid2==0)//child
              {
                int childstate2;
                childstate2=0;//the child is monitoring a process currently
                int pipeindex=numchildren-1;
                write(pipe_from_child[pipeindex][1],&childstate2,4);//write four bytes to the pipe,indicates the child is being used currently
                close(pipe_from_child[pipeindex][0]);//close the read end of the pipe
                close(pipe_from_parent[pipeindex][1]);//close the write end of the pipe,this pipe is used to read message from the parent
                int processid;
                processid=pidlist[i_5];
                char *processname;
                processname=str[i_4];
                int processtime;
                processtime=pro_time[i_4];

                processmonitor(processid,processname);
                int killstatus;//0 means the process is killed, 1 means no process was killed
                killstatus=processkiller(processid,processtime);

                //successfully killed a process
                if(killstatus==0)
                {
                  processkilled(processid,processname,processtime);
                  //print out the time when the process was killed
                }
                childstate=1;//the child is done monitoring
                write(pipe_from_child[pipeindex][1],&childstate,4);//write another 4 bytes to the pipe, indicates that the child is free to use

                while(1)
                {
                  int messagesize;
                  read(pipe_from_parent[pipeindex][0],&messagesize,4);//this is used to received the size of the message
                  read(pipe_from_parent[pipeindex][0],processname,messagesize);//this is used to receive the message
                  read(pipe_from_parent[pipeindex][0],&processid,4);//this is used to receive the process id
                  read(pipe_from_parent[pipeindex][0],&processtime,4);//this is used to received the processtime
                  write(pipe_from_child[pipeindex][1],&childstate,4);//write four bytes to the pipe,indicates the child is being used currently
                  processmonitor(processid,processname);//start monitoring the process
                  killstatus=processkiller(processid,processtime);//kill the process
                  //successfully killed a process
                  if(killstatus==0)
                  {
                    processkilled(processid,processname,processtime);
                    //print out the time when the process was killed
                  }
                  childstate=1;//the child is done monitoring
                  write(pipe_from_child[pipeindex][1],&childstate,4);//write another 4 bytes to the pipe, indicates that the child is free to use
                }//end of while
              }
              //getting out of children
              children_pid[numchildren-1]=(int)pid2;
            LABLE1:
              nothing=0;//do nothing here
              fflush(stdout);
            }
            //this part means no free child is found, you need to creat a new child to kill the process
          }
        }
        if(num_per_process==0)
        {
          //checkprocessfound[i_4]=0;
          if(printchecker<num_line)
          {
            printchecker++;
            processnotfound(str[i_4]);
          }
        }
      }//if(checkprocessfound[i_4]!=1)
    }//for(i_4=0;i_4<num_line;i_4++)
  }//this part is for checking the original file every 5 seconds
  return 0;
}

void receive_testfile(int fs)
{
  int send_message;
  for(send_message=0;send_message<num_line;send_message++)
  {
    read(fs,&pro_time[send_message],4);
    read(fs,str[send_message],50);
  }
}

void processmonitor(int a,char *b)
{
  time_t timer;
  time(&timer);
  char *time=ctime(&timer);
  time[strlen(time) - 1 ] = '\0';
  char info[300];
  sprintf(info,"[%s] %s %s %s %d %s %s %s",time," Info: Initializing monitoring of process '",b,"' (PID ",a,") on node ",hostname,"\n");
  write(fromserver,info,256);
}

void processkilled(int a,char *b,int num_time)
{
  time_t timer;
  time(&timer);
  char *time=ctime(&timer);
  time[ strlen(time) - 1 ] = '\0';
  char info[300];
  sprintf(info,"[%s] %s %d %s %s %s %d %s %s %s",time," Action: PID ",a,"(",b,") killed after ",num_time," seconds on node ",hostname,"\n");
  write(fromserver,info,256);
}

void processnotfound(char *b)
{
  time_t timer;
  time(&timer);
  char *time=ctime(&timer);
  time[ strlen(time) - 1 ] = '\0';
  char info[300];
  sprintf(info,"[%s] %s %s %s %s %s",time," Info: No'",b,"' processes found on node ",hostname,"\n");
  write(fromserver,info,256);
}

//this function get the pid of a specific process into an array of integers
int pidFinder(int pidlist[],FILE *commandReturn)
{
  int i=0;//this is a counter
  //this while loop stores of the pids of a specific process into an integer array
  while(fscanf(commandReturn,"%d",&pidlist[i])!=EOF)
  {
    i++;
  }
  return i;
}

//this function is used to kill processes
int processkiller(int pid,int num_time)
{
  int i;
  sleep(num_time);//sleep for num_seconds and wake up
  i=kill((pid_t)pid,9);//kill a specific child
  return i;
}

//this function is used to check if a specific integer is in an integer array
int isvalueinarray(int val, int *arr, int size)
{
  int i;
  for (i=0; i < size; i++)
  {
    if (arr[i] == val)
    {
      return 1;
    }
  }
  return 0;
}

//this is the signal handler
void sig_handler(int sig)
{
  if(sig==SIGHUP)
  {
    if(getpid()==ppid)
    {
      rereadflag=1;//when catch a SIGHUP signal,set the rereadflag to 1,which means it is needed to reread the file
    }
  }
  if(sig==SIGINT)
  {
    if(getpid()==ppid)
    {
      logscan();//quit the programss
    }
  }
}

void logscan()
{
  int i;
  for(i=0;i<numchildren;i++)
  {
    kill((pid_t)children_pid[i],9);
  }
  //kill all its children
  kill((pid_t)ppid,9);
}

void re_read()
{
  printchecker=0;
}
