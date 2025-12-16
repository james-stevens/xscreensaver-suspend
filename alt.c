#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <syslog.h>
#include <signal.h>
#include <sys/wait.h>

#define ZERO(A) memset(&A,0,sizeof(A))
char suspend_command[250];

#define MONITOR_POWER_DOWN 10
#define NEED_SYSTEM_RUNNING 3



int interupt=0;
void sig(int s) { interupt = s; }


int child_process()
{
char line[200];
FILE *p = NULL;

	while(!interupt) {
		if ((p = popen("exec xscreensaver-command --watch","r"))==NULL) {
			sleep(1); continue; }

		while(fgets(line,sizeof(line),p)!=NULL) {
			syslog(LOG_INFO,"%s",line);
			if (strncmp(line,"RUN 0 ",6)==0) {
				syslog(LOG_INFO,"Run-0 detected");
				exit(MONITOR_POWER_DOWN);
				}
			}
		pclose(p);
		}
	exit(0);
	return 0;
}


int usage()
{
	puts("xscreensaver-suspend [ -t <secs-after-monitor-power-off> ] [ -s <suspend-command> ]");
	puts("default: 30mins & 'systemctl suspend'");
	exit(0);
}



int main(int argc, char * argv[])
{
int is_system_running = 0;
struct sigaction sa;
int time_to_suspend = 60*30;
pid_t child = 0;

	ZERO(sa);
	sa.sa_handler = sig;
	sigaction(SIGALRM,&sa,NULL);
	sigaction(SIGINT,&sa,NULL);
	sigaction(SIGTERM,&sa,NULL);
	sigaction(SIGPIPE,&sa,NULL);
	strcpy(suspend_command,"systemctl suspend");

    int opt;
    while ((opt=getopt(argc,argv,"t:s:")) > 0)
        {
        switch(opt)
            {
            case 't': time_to_suspend = atoi(optarg); break;
            case 's': strcpy(suspend_command,optarg); break;
            default: usage(); exit(0);
            }
		}

	openlog("xscreensaver-suspend",LOG_PID,LOG_USER);

	while(!interupt) {
		if (!child) {
			if (is_system_running < NEED_SYSTEM_RUNNING) {
				int ret = system("systemctl is-system-running");
				syslog(LOG_INFO,"is-system-running %d, ret=%d\n",is_system_running,ret);
				is_system_running++;
				}
			if (is_system_running >= NEED_SYSTEM_RUNNING) {
				child = fork();
				syslog(LOG_INFO,"forked child at %d",child);
				if (child == 0) exit(child_process());
				}
			}
		else {
			pid_t died;
			int status;
			if ((died = waitpid(-1,&status,WNOHANG)) > 0) {
				syslog(LOG_INFO,"Child died");
				child = 0; is_system_running = 0;
				if (status == MONITOR_POWER_DOWN)  {
				    syslog(LOG_INFO,"status = %d - Running '%s'",status,suspend_command);
					system(suspend_command);
					}
				}
			}
		sleep(1);
		}
	return 0;
}
