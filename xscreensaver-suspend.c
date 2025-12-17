#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <syslog.h>
#include <signal.h>
#include <errno.h>

#define ZERO(A) memset(&A,0,sizeof(A))
char suspend_command[250];

int is_system_running = 0;
#define NEED_SYSTEM_RUNNING 2


FILE *p = NULL;


int interupt=0;
void sig(int s)
{ 
	syslog(LOG_INFO,"signal %d",s);
	if (s!=SIGALRM) { interupt=s; return; }

	system("exec killall xscreensaver-command");
	sleep(1);

	alarm(0);

	syslog(LOG_INFO,"SIGALRM - Running '%s'",suspend_command);

	pclose(p); p = NULL;
	is_system_running = 0;

	system(suspend_command);
	sleep(1);
}



int usage()
{
	puts("xscreensaver-suspend [ -t <secs-after-monitor-power-off> ] [ -s <suspend-command> ]");
	puts("default: 30mins & 'systemctl suspend'");
	exit(0);
}



int main(int argc, char * argv[])
{
char line[200];
struct sigaction sa;
int time_to_suspend = 60*30;

	ZERO(sa);
	sa.sa_handler = sig;
	sigaction(SIGALRM,&sa,NULL);
	sigaction(SIGINT,&sa,NULL);
	sigaction(SIGTERM,&sa,NULL);
	strcpy(suspend_command,"exec systemctl suspend");

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
		while (!p) {
			int ret = system("systemctl is-system-running | logger -t xscreensaver-suspend -i");
			if (ret >= 0) is_system_running++; else is_system_running=0;
			syslog(LOG_INFO,"is-system-running %d of %d, ret=%d\n",is_system_running,NEED_SYSTEM_RUNNING,ret);
			if (is_system_running >= NEED_SYSTEM_RUNNING) {
				syslog(LOG_INFO,"opening pipe");
				p = popen("xscreensaver-command --watch","r");
				syslog(LOG_INFO,"pipe result: %s",strerror(errno));
				}
			sleep(1);
			}

		if (!p) continue;

		syslog(LOG_INFO,"looping");
		while((p)&&(fgets(line,sizeof(line),p)!=NULL)) {
			syslog(LOG_INFO,"%s",line);
			if (strncmp(line,"RUN 0 ",6)==0) {
				alarm(time_to_suspend);
				syslog(LOG_INFO,"suspend in %d secs",time_to_suspend);
				}
			else {
				int i = alarm(0);
				if (i) syslog(LOG_INFO,"alarm cancelled with %d secs left",i);
				}
			}

		if (p) { pclose(p); p = NULL; is_system_running = 0; }
		}
	return 0;
}
