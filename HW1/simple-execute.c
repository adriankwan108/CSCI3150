#include <stdlib.h>
#include <stdio.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

//光復香港 時代革命


void new_execute(char **args, int argc)
{
	int counter=0, p1[2], p2[2];
	int count=argc-1;

	int child_pid, pid1, pid2;
	int wait_return, wait1, wait2;
	int status, status1, status2;

	int a, m, n, t;

	char **tmp;
    char ** str1;
    char ** str2;
    char ** str3;
	char ** arg;

	for(int i=0;i<count;i++)
	{
		if(strcmp(args[i],"|")==0)
        {
            counter++;
        }
	}
	switch (counter) {
		case 0:
		if( (child_pid = fork()) < 0 )
        {
			printf("fork() error \n");
		}
		else if (child_pid == 0 )
		{
			if ( execvp(args[0], args) < 0)
			{
				printf("execvp() error \n");
				exit(-1);
			}
		}else
		{
			if ( (wait_return = wait(&status) ) < 0 )
				printf("wait() error \n");
		}
			break;

		case 1:
            a=0;
            tmp = malloc(30*sizeof(char*));
            while(strcmp(args[a], "|") != 0)
            {
                tmp[a] = args[a];
                a++;
            } //parse

            str1 = malloc(a*sizeof(char*));

            for(int i=0;i<a;i++)
            {
                str1[i] = args[i];
            }

            str1[a]=NULL;
            str2 = malloc((argc-a+1)*sizeof(char*));
            for(int l=0;a<argc;l++,a++)
            {
                str2[l]=args[a];
            }

            if((child_pid=fork()) < 0)
            {
                printf("Fork error\n");
            }
            else if(child_pid == 0)
            {
                if(pipe(p1)<0)
                {
                    printf("Pipe Fail LUL~\n");
                    exit(-1);
                }
                if((pid1=fork())>0)
                {
                    close(STDOUT_FILENO);
                    dup(p1[1]);
                    close(p1[0]);
                    close(p1[1]);

                    if(execvp(str1[0],str1)<0)
                    {
                        printf("hi, I am execvp error\n");
                        exit(-1);
                    }
                }
                else if(pid1==0)
                {
                    close(STDIN_FILENO);
                    dup(p1[0]);
                    close(p1[1]);
                    close(p1[0]);
                    if(execvp(str2[0],str2)<0)
                    {
                        printf("hi, I am execvp error\n");
                        exit(-1);
                    }
                }
                else
                {
                    printf("xd~ here is fork() error\n");
                    exit(-1);
                }
                free(str1);
                free(str2);
            }
            else if((wait_return = wait(&status)) < 0)
                printf("wait error, you should probably wait more");
			break;

		case 2:
            m=0;
            n=0;
            tmp = malloc(30*sizeof(char*));
            while(strcmp(args[m], "|") != 0)
            {
              tmp[m] = args[m];
              m++;
            }

            str1 = malloc(m*sizeof(char*));
            for(int i=0;i<m;i++)
            {
                str1[i] = args[i];
            }
            str1[m]=NULL;

            m++;
			t=m;

            while(strcmp(args[m], "|") != 0)
            {
              tmp[n]=args[m];
              n++;m++;
            }

            str2 = malloc(n*sizeof(char*));
            for(int i=0;i<n;i++,t++)
            {
                str2[i] = args[t];
            }
            str2[n]=NULL;
            m++;

			str3 = malloc((argc-m+1)*sizeof(char*));
            for(n=0;m<argc;m++,n++)
            {
              str3[n]=args[m];
            }

            if((child_pid = fork()) < 0)
            {
                printf("Fork error\n");
            }
            else if(child_pid == 0)
            {
                if(pipe(p1)<0)
                {
                    printf("Failed p1 create\n");
                    exit(-1);
                }

                if(pipe(p2)<0)
                {
                    printf("Failed p2 create\n");
                    exit(-3);
                }

                if((pid1=fork())<0)   //fork
                {
                    printf("fork1 error\n");
                    exit(-2);
                }

                if(pid1==0)  //first fork OK!
                {
                    if((pid2=fork())<0)  //check second fork
                    {
                        printf("fork2 error\n");
                        exit(-2);
                    }
                    if(pid2==0)
                    {
                        close(p1[0]);
                        close(STDOUT_FILENO);
                        dup(p1[1]);
                        close(p2[1]);
                        close(p2[0]);

                        if(execvp(str1[0],str1)<0)
                        {
                            printf("execvp1 error\n");
                            exit(-1);
                        }
                    }
                    else   //pid2 > 1
                    {
                        close(p1[1]);
                        close(STDIN_FILENO);
                        dup(p1[0]);
                        close(p2[0]);
                        close(STDOUT_FILENO);
                        dup(p2[1]);

                        if(execvp(str2[0],str2)<0)
                        {
                            printf("execvp2 error\n");
                            exit(-1);
                        }
                    }
                }
                else
                {
                    //parent
                    close(p2[1]);
                    close(STDIN_FILENO);
                    dup(p2[0]);
                    close(p1[1]);
                    close(p1[0]);

                    if(execvp(str3[0],str3)<0)
                    {
                        printf("execvp3 error\n");
                        exit(-1);
                    }
                }
            free(str1);
            free(str2);
            free(str3);
            }
            else if((wait_return = wait(&status)) < 0)
            {
                printf("wait2 error\n");
            }
			break;
	} //switch close
}

int shell_execute(char ** args, int argc)
{
	if ( strcmp(args[0], "EXIT") == 0 )
		return -1;

	new_execute(args, argc);

	return 0;

}

//五大訴求 缺一不可
