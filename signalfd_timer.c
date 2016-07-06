#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <time.h>

#define HAVE_SYS_SIGNALFD_H

#ifdef HAVE_SYS_SIGNALFD_H
#include <sys/signalfd.h>
#else
#include <sys/syscall.h>
#define __NR_signalfd           (0x900000 + 349)
#define __NR_signalfd4          (0x900000 + 355)

#define SFD_NONBLOCK	(04000)
struct signalfd_siginfo {
	uint32_t ssi_signo;
	int32_t ssi_errno;
	int32_t ssi_code;
	uint32_t ssi_pid;
	uint32_t ssi_uid;
	int32_t ssi_fd;
	uint32_t ssi_tid;
	uint32_t ssi_band;
	uint32_t ssi_overrun;
	uint32_t ssi_trapno;
	int32_t ssi_status;
	int32_t ssi_int;
	uint64_t ssi_ptr;
	uint64_t ssi_utime;
	uint64_t ssi_stime;
	uint64_t ssi_addr;
	uint16_t ssi_addr_lsb;
	uint8_t __pad[46];
};

static inline int signalfd(int __fd, const sigset_t *__mask, int __flags)
{
	//return syscall(__NR_signalfd4, __fd, __mask, _NSIG / 8, __flags);
	return syscall(__NR_signalfd, __fd, __mask, _NSIG / 8);
}
#endif

static int handler_run;
static const union sigval my_sigval = { .sival_int = 42, };

#define handle_error(msg) \
	do { perror(msg); exit(EXIT_FAILURE); } while (0)

int main(int argc, char **argv)
{
	struct signalfd_siginfo fdsi;
	struct itimerspec its;
	struct sigevent sev;
	timer_t timerid;
	sigset_t mask;
	ssize_t s;
	int sfd;

	if ((argc != 2) && (argc != 4)) {
		fprintf(stderr, "%s init-secs [interval-secs exp]\n",
				argv[0]);
		exit(EXIT_FAILURE);
	}

	memset(&sev, 0, sizeof(sev));
	sev.sigev_notify = SIGEV_SIGNAL;
	sev.sigev_signo = SIGRTMIN;

	if (timer_create(CLOCK_MONOTONIC, &sev, &timerid) == -1)
		handle_error("timer_create");

	/* Inhibit default SIGRTMIN handling */
	sigemptyset(&mask);
	sigaddset(&mask, SIGRTMIN);
	if (sigprocmask(SIG_BLOCK, &mask, NULL) == -1)
		handle_error("sigprocmask");

	sfd = signalfd(-1, &mask, 0);
	if (sfd == -1)
		handle_error("signalfd");

	its.it_value.tv_sec = atoi(argv[1]);
	its.it_value.tv_nsec = 0;
	its.it_interval.tv_sec = 1;
	its.it_interval.tv_nsec = 0;
	
	/* Re-arm timer */
	if (timer_settime(timerid, 0, &its, NULL) == -1)
		handle_error("timer_settime");

	/* Block until timer expiration */
	while (1) {
		s = read(sfd, &fdsi, sizeof(struct signalfd_siginfo));
		if (s != sizeof(struct signalfd_siginfo))
			handle_error("read");
		
		printf("expired\n");
	}

	assert(fdsi.ssi_code == SI_TIMER);
	assert(fdsi.ssi_ptr ==
			(unsigned long)my_sigval.sival_ptr); /* Succeeds */
	assert(fdsi.ssi_int == my_sigval.sival_int); /* Fails */

	return EXIT_SUCCESS;
}
