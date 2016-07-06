#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>        /* Definition of uint64_t */

#define HAVE_SYS_TIMERFD_H

#ifdef HAVE_SYS_TIMERFD_H
#include <sys/timerfd.h>
#else
#define __NR_timerfd_create          (0x900000 + 350)
#define __NR_timerfd_settime        (0x900000 + 353)
#define TFD_NONBLOCK (04000)
#define TFD_TIMER_ABSTIME (1 << 0)

static inline int timerfd_create(clockid_t __clock_id, int __flags)
{
	return syscall(__NR_timerfd_create, __clock_id, __flags);
}

static inline int timerfd_settime(int __ufd, int __flags,
		__const struct itimerspec *__utmr, struct itimerspec *__otmr)
{
	return syscall(__NR_timerfd_settime, __ufd, __flags, __utmr, __otmr);
}

#endif

#define handle_error(msg) \
	do { perror(msg); exit(EXIT_FAILURE); } while (0)

static void
print_elapsed_time(void)
{
	static struct timespec start;
	struct timespec curr;
	static int first_call = 1;
	int secs, nsecs;

	if (first_call) {
		first_call = 0;
		if (clock_gettime(CLOCK_MONOTONIC, &start) == -1)
			handle_error("clock_gettime");
	}

	if (clock_gettime(CLOCK_MONOTONIC, &curr) == -1)
		handle_error("clock_gettime");

	secs = curr.tv_sec - start.tv_sec;
	nsecs = curr.tv_nsec - start.tv_nsec;
	if (nsecs < 0) {
		secs--;
		nsecs += 1000000000;
	}
	printf("%d.%03d: ", secs, (nsecs + 500000) / 1000000);
}

int main(int argc, char *argv[])
{
	struct itimerspec new_value;
	int max_exp, fd;
	struct timespec now;
	uint64_t exp, tot_exp;
	ssize_t s;

	if ((argc != 2) && (argc != 4)) {
		fprintf(stderr, "%s init-secs [interval-secs max-exp]\n",
				argv[0]);
		exit(EXIT_FAILURE);
	}

	new_value.it_value.tv_sec = atoi(argv[1]);
	new_value.it_value.tv_nsec = 0;
	if (argc == 2) {
		new_value.it_interval.tv_sec = 0;
		max_exp = 1;
	} else {
		new_value.it_interval.tv_sec = atoi(argv[2]);
		max_exp = atoi(argv[3]);
	}
	new_value.it_interval.tv_nsec = 0;

	fd = timerfd_create(CLOCK_REALTIME, 0);
	if (fd == -1)
		handle_error("timerfd_create");
	
	if (timerfd_settime(fd, 0, &new_value, NULL) == -1)
		handle_error("timerfd_settime");
	
	printf("timer started\n");
	print_elapsed_time();

	for (tot_exp = 0; tot_exp < max_exp;) {
		s = read(fd, &exp, sizeof(uint64_t));
		if (s != sizeof(uint64_t))
			handle_error("read");

		tot_exp += exp;
		print_elapsed_time();
		printf("read: %llu; total=%llu\n",
				(unsigned long long) exp,
				(unsigned long long) tot_exp);
	}

	exit(EXIT_SUCCESS);
}
