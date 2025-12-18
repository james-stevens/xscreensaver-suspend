#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <syslog.h>
#include <signal.h>
#include <errno.h>
#include <sys/stat.h>
#include <dirent.h>
#include <ctype.h>
#include <sys/types.h>

#define SUSPEND_COMMAND "exec systemctl suspend"

#define ZERO(A) memset(&A,0,sizeof(A))

int is_system_running = 0;
#define NEED_SYSTEM_RUNNING 2

FILE *watch_pipe = NULL;


#define MAX_PARTS 5

void kill_watch_by_pid()
{
DIR *d = opendir("/proc");
struct dirent *ent;
char * parts[MAX_PARTS];

    while((ent = readdir(d))!=NULL) {
        if (!((isdigit(ent->d_name[0]))&&(ent->d_type==DT_DIR))) continue;

        struct stat st;
        char file[PATH_MAX];
        sprintf(file,"/proc/%s/stat",ent->d_name);
        if (stat(file,&st)) continue;

        FILE *fp = fopen(file,"r");
        if (!fp) continue;
        char line[1000];
        if (fgets(line,sizeof(line),fp)==NULL) { fclose(fp); continue; }
        fclose(fp);
        char *sv=NULL; int l=0;
        for(char * cp=strtok_r(line," ",&sv);cp;cp=strtok_r(NULL,", ",&sv)) {
            parts[l++] = cp; if (l==MAX_PARTS) break;
            }

        if (strcmp(parts[1],"(xscreensaver-co)")) continue;
        if (atoi(parts[3])==getpid()) {
            kill(atoi(ent->d_name),SIGTERM);
            break;
            }
        }
    closedir(d);
}


int interupt=0;
void sig(int s)
{
	syslog(LOG_DEBUG,"signal %d",s);
	if (s!=SIGALRM) { interupt=s; return; }

	kill_watch_by_pid();
	alarm(0);
	syslog(LOG_INFO,"SIGALRM - Running '%s'",SUSPEND_COMMAND);
	pclose(watch_pipe); watch_pipe = NULL;
	is_system_running = 0;
	system(SUSPEND_COMMAND);
	sleep(1);
}



int usage()
{
	puts("xscreensaver-suspend [ -t <secs-after-monitor-power-off> ] &");
	puts("default: 30mins");
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

    int opt;
    while ((opt=getopt(argc,argv,"t:")) > 0)
        {
        switch(opt)
            {
            case 't': time_to_suspend = atoi(optarg); break;
            default: usage(); exit(0);
            }
		}

	openlog("xscreensaver-suspend",LOG_PID,LOG_USER);

	while(!interupt) {
		while ((!interupt)&&(!watch_pipe)) {
			int ret = system("systemctl is-system-running | logger -t xscreensaver-suspend -i");
			if (ret >= 0) is_system_running++; else is_system_running=0;
			syslog(LOG_DEBUG,"is-system-running %d of %d, ret=%d\n",is_system_running,NEED_SYSTEM_RUNNING,ret);
			if (is_system_running >= NEED_SYSTEM_RUNNING) {
				watch_pipe = popen("exec xscreensaver-command --watch","r");
				syslog(LOG_DEBUG,"pipe result: %s, watch_pipe is NULL -> %d",strerror(errno),(watch_pipe==NULL));
				}
			sleep(1);
			}

		if (!watch_pipe) continue;

		syslog(LOG_DEBUG,"looping");
		while((!interupt)&&(watch_pipe)&&(fgets(line,sizeof(line),watch_pipe)!=NULL)) {
			syslog(LOG_DEBUG,"%s",line);
			if (strncmp(line,"RUN 0 ",6)==0) {
				alarm(time_to_suspend);
				syslog(LOG_INFO,"suspend in %d secs",time_to_suspend);
				}
			else {
				int i = alarm(0);
				if (i) syslog(LOG_INFO,"alarm cancelled with %d secs left",i);
				}
			}

		if (watch_pipe) {
			kill_watch_by_pid();
			pclose(watch_pipe);
			watch_pipe = NULL;
			is_system_running = 0;
			}
		}
	return 0;
}
