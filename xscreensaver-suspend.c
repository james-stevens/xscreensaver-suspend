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

#define SUSPEND_COMMAND "exec systemctl suspend"

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


int have_run_zero()
{
int ret = 0;

	if (end == buf) return 0;
	char *nxt,*pos = buf;
	while ((pos < end)&&((nxt = strchr(pos,'\n'))!=NULL)) {
		*nxt = 0;
		syslog(LOG_DEBUG,"xscreensaver reported '%s'",pos);

		if (strncmp(pos,"RUN 0 ",6)==0) ret = 1; else ret = 0;
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
	syslog(LOG_NOTICE,"Running '%s'",SUSPEND_COMMAND);
	system(SUSPEND_COMMAND);
	sleep(1);
}



int usage()
{
	puts("xscreensaver-suspend [ -t <secs-after-monitor-stand-by> ] [ -l <log-level> ] [ -w <watch-command> ] [ -e ] &");
	puts("    -t <secs> ... default: suspend 30mins after monitor stand-by");
	puts("    -l <log-level> ... use `-l 5` to only log significant state changes, LOG_NOTICE");
	puts("    -w <cmd> ... watch command is `exec xscreensaver-command --watch`, using other values is useful for debug only");
	puts("    -e ... also syslog to stderr");
	exit(0);
}



int main(int argc, char * argv[])
{
char watch_command[250];
struct sigaction sa;
int time_to_suspend = 60*30;

	openlog("xscreensaver-suspend",LOG_PID,LOG_USER);

	ZERO(sa);
	sa.sa_handler = sig;
	sigaction(SIGINT,&sa,NULL);
	sigaction(SIGTERM,&sa,NULL);

	strcpy(watch_command,"exec xscreensaver-command --watch");

    int opt;
    while ((opt=getopt(argc,argv,"t:l:ew:")) > 0)
        {
        switch(opt)
            {
            case 'w': strcpy(watch_command,optarg); break;
            case 'l': LOG_UPTO(atoi(optarg)); break;
            case 't': time_to_suspend = atoi(optarg); break;
			case 'e': openlog("xscreensaver-suspend",LOG_PID|LOG_PERROR,LOG_USER); break;
            default: usage(); exit(0);
            }
		}

	while(!interupt) {
		time_t now = time(NULL);
		if ((supend_at)&&(now >= supend_at)) do_suspend();

		pid_t pid; int ret;
		while((pid=waitpid(-1,&ret,WNOHANG)) > 0)
			if (pid==watch_pid) end_watcher();

		if (!watch_pipe_fd) {
			ret = system("systemctl is-system-running | logger -t xscreensaver-suspend -i");
			if (ret >= 0) is_system_running++; else is_system_running=0;
			syslog(LOG_DEBUG,"is-system-running %d of %d, ret=%d\n",is_system_running,NEED_SYSTEM_RUNNING,ret);

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

		if (time(NULL) > now+(MAX_TIME_GAP)) {
			syslog(LOG_DEBUG,"manual suspend suspected\n");
			end_watcher();
			continue; }

		if (ret == 0) continue;
		if (read_more() < 0) { end_watcher(); continue; }

		if (have_run_zero()) {
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
