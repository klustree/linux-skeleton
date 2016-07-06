#include <signal.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

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
#define handle_error(msg) \
	do { perror(msg); exit(EXIT_FAILURE); } while (0)

int main(int argc, char *argv[])
{
	sigset_t mask;
	int sfd;
	struct signalfd_siginfo fdsi;
	ssize_t s;

	sigemptyset(&mask);
	sigaddset(&mask, SIGINT);
	sigaddset(&mask, SIGQUIT);

	/* Block signals so that they aren't handled
	 *               according to their default dispositions */

	if (sigprocmask(SIG_BLOCK, &mask, NULL) == -1)
		handle_error("sigprocmask");

	sfd = signalfd(-1, &mask, 0);
	if (sfd == -1)
		handle_error("signalfd");

	for (;;) {
		s = read(sfd, &fdsi, sizeof(struct signalfd_siginfo));
		if (s != sizeof(struct signalfd_siginfo))
			handle_error("read");

		if (fdsi.ssi_signo == SIGINT) {
			printf("Got SIGINT\n");
		} else if (fdsi.ssi_signo == SIGQUIT) {
			printf("Got SIGQUIT\n");
			exit(EXIT_SUCCESS);
		} else {
			printf("Read unexpected signal\n");
		}
	}
}
