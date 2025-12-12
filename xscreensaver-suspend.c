#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <syslog.h>
#include <signal.h>

#define ZERO(A) memset(&A,0,sizeof(A))
char suspend_command[250];



int interupt=0;
void sig(int s)
{ 
	if (s!=SIGALRM) { interupt=1; return; }

	alarm(0);
	syslog(LOG_INFO,"SIGALRM - Running '%s'",suspend_command);
	system(suspend_command);
}



int usage()
{
	puts("xscreensaver-suspend [ -t <secs-after-monitor-power-off> ] [ -s <suspend-command> ]");
	puts("default: 30mins & 'systemctl suspend'");
	exit(0);
}



int main(int argc, char * argv[])
{
FILE *p = NULL;
char line[200];
struct sigaction sa;
int time_to_suspend = 60*30;

	signal(SIGPIPE,SIG_IGN);
	ZERO(sa);
	sa.sa_handler = sig;
	sigaction(SIGALRM,&sa,NULL);
	sigaction(SIGINT,&sa,NULL);
	sigaction(SIGTERM,&sa,NULL);
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
		if ((p = popen("exec xscreensaver-command --watch","r"))==NULL) break;
		while(fgets(line,sizeof(line),p)!=NULL) {
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
		pclose(p);
		}
	return 0;
}
