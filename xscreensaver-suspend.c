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
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>

#define STRCPY(D,S) strncpy(D,S,sizeof(D)-1)
#define SUSPEND_COMMAND "exec systemctl suspend"
#define CHECK_COMMAND "systemctl is-system-running | logger -t xscreensaver-suspend -i"
#define WATCH_COMMAND "exec xscreensaver-command --watch"
#define TRIGGER_EVENT "RUN 0 "

char suspend_command[250],check_command[250],watch_command[250],trigger_event[250];

#define ZERO(A) memset(&A,0,sizeof(A))

#define POLL_TIMEOUT 10
#define MAX_TIME_GAP (POLL_TIMEOUT*3)

int is_system_running = 0;
#define NEED_SYSTEM_RUNNING 2

int watch_pipe_fd = 0;
pid_t watch_pid = 0;
time_t supend_at = 0;

#define MAXBUF 1000
char buf[MAXBUF],*end=buf;


int check_trigger_event()
{
int ret = 0;
int len_trig = strlen(trigger_event);

	if (end == buf) return 0;
	char *nxt,*pos = buf;
	while ((pos < end)&&((nxt = strchr(pos,'\n'))!=NULL)) {
		*nxt = 0;
		syslog(LOG_DEBUG,"xscreensaver reported '%s'",pos);
		if (strncmp(pos,trigger_event,len_trig)==0) ret = 1; else ret = 0;
		pos = nxt+1;
		}

	if (pos >= end) pos = end = buf;
	else if (pos > buf) {
		int len = end - pos;
		memmove(buf,pos,len);
		end = buf + len;
		}

	return ret;
}



int read_more()
{
	int ret = read(watch_pipe_fd,buf,sizeof(buf)-(end-buf));
	if (ret >= 0) end += ret;
	*end = 0;
	return ret;
}


int read_poll(int fd, time_t secs)
{
struct timeval tv;
fd_set read_fds;

    FD_ZERO(&read_fds);
    FD_SET(fd,&read_fds);

    tv.tv_sec = secs; tv.tv_usec = 0;
    return select(fd+1,&read_fds,NULL,NULL,&tv);
}


int my_popen(const char *command)
{
int pfd[2];
pid_t child_pid;

    if (pipe(pfd) == -1) return -1;
    if ((child_pid = fork()) < 0) return -1;

    if (child_pid == 0) {  // Child
		close(pfd[0]);
		dup2(pfd[1], STDOUT_FILENO);
		close(pfd[1]);
        execl("/bin/sh", "sh", "-c", command, (char *)NULL);
        exit(1);  // If execl fails
    } else {  // Parent
		close(pfd[1]);
		watch_pid = child_pid;
		return pfd[0];
		}
	return -1; // never gets here
}


void end_watcher()
{
	if (watch_pid) kill(watch_pid,SIGTERM);
	watch_pid = 0;
	if (watch_pipe_fd) close(watch_pipe_fd);
	watch_pipe_fd = 0;
	is_system_running = 0;
	supend_at = 0;
}


int interupt=0;
void sig(int s) { interupt=s; return; }


void do_suspend()
{
	end_watcher();
	syslog(LOG_NOTICE,"Suspending with '%s'",suspend_command);
	system(suspend_command); // seems this call doesn't return until the PC wakes
	sleep(1);
}



int usage()
{
	puts("xscreensaver-suspend &");
	puts("    -t <secs> ... default: suspend 30mins after monitor stand-by");
	puts("    -l <log-level> ... use `-l 5` to only log significant state changes, LOG_NOTICE & above");
	puts("    -w <cmd> ... watch command is `exec xscreensaver-command --watch`");
	puts("    -c <cmd> ... check command is `systemctl is-system-running`");
	puts("    -s <cmd> ... suspend command is `exec systemctl suspend`");
	puts("    -r <event> ... trigger event, default is `RUN 0 `");
	puts("    -e ... also syslog to stderr");
	exit(0);
}



int main(int argc, char * argv[])
{
struct sigaction sa;
int time_to_suspend = 60*30;

	strcpy(suspend_command,SUSPEND_COMMAND);
	strcpy(check_command,CHECK_COMMAND);
	strcpy(watch_command,WATCH_COMMAND);
	strcpy(trigger_event,TRIGGER_EVENT);

	openlog("xscreensaver-suspend",LOG_PID,LOG_USER);

	ZERO(sa);
	sa.sa_handler = sig;
	sigaction(SIGINT,&sa,NULL);
	sigaction(SIGTERM,&sa,NULL);

	strcpy(watch_command,"exec xscreensaver-command --watch");

    int opt;
    while ((opt=getopt(argc,argv,"t:l:ew:r:c:s:")) > 0)
        {
        switch(opt)
            {
            case 'r': STRCPY(trigger_event,optarg); break;
            case 'c': STRCPY(check_command,optarg); break;
            case 's': STRCPY(suspend_command,optarg); break;
            case 'w': STRCPY(watch_command,optarg); break;
            case 'l': LOG_UPTO(atoi(optarg)); break;
            case 't': time_to_suspend = atoi(optarg); break;
			case 'e': openlog("xscreensaver-suspend",LOG_PID|LOG_PERROR,LOG_USER); break;
            default: usage(); exit(0);
            }
		}

	while(!interupt) {
		time_t now = time(NULL);
		if ((supend_at)&&(now >= supend_at)) do_suspend();

		if (time(NULL) > now+(MAX_TIME_GAP)) {
			syslog(LOG_NOTICE,"External suspend suspected\n");
			end_watcher();
			}

		pid_t pid; int ret;
		while((pid=waitpid(-1,&ret,WNOHANG)) > 0)
			if (pid==watch_pid) end_watcher();

		if (!watch_pipe_fd) {
			ret = system(check_command);
			if (ret >= 0) is_system_running++; else is_system_running=0;
			syslog(LOG_DEBUG,"check cmd %d of %d, ret=%d\n",is_system_running,NEED_SYSTEM_RUNNING,ret);

			if (is_system_running >= NEED_SYSTEM_RUNNING) {
				watch_pipe_fd = my_popen(watch_command);
				syslog(LOG_DEBUG,"pipe result: %s, watch_pipe_fd %d",(watch_pipe_fd < 0)?strerror(errno):"OK",watch_pipe_fd);
				}

			sleep(1);
			continue;
			}

		if (!watch_pipe_fd) continue; // just in case!

		ret = read_poll(watch_pipe_fd,POLL_TIMEOUT);
		if (ret < 0) { end_watcher(); continue; }
		if (ret == 0) continue;

		if (read_more() < 0) { end_watcher(); continue; }

		if (check_trigger_event()) {
			syslog(LOG_NOTICE,"suspend in %d secs",time_to_suspend);
			supend_at = now+time_to_suspend;
			}
		else {
			if (supend_at)
				syslog(LOG_NOTICE,"suspend cancelled with %ld secs left",(supend_at - now));
			supend_at = 0;
			}
		}

	end_watcher();
	return 0;
}
