#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>	       /* Definition of uint64_t */

#define HAVE_SYS_EVENTFD_H

#ifdef HAVE_SYS_EVENTFD_H
#include <sys/eventfd.h>
#else
#define __NR_eventfd2		(0x900000 + 356)
#define EFD_SEMAPHORE		(1)
#define EFD_NONBLOCK		(04000)
#define eventfd_t			uint64_t

static inline int eventfd_write(int fd, eventfd_t value)
{
	return write(fd, &value, sizeof(eventfd_t)) != 
		sizeof(eventfd_t) ? -1 : 0;
}

static inline int eventfd_read(int fd, eventfd_t *value)
{
	return read(fd, value, sizeof(eventfd_t)) !=
			sizeof(eventfd_t) ? -1 : 0;
}

static inline int eventfd(unsigned int initval, int flags)
{
	return syscall(__NR_eventfd2, initval, flags);
}
#endif

#define handle_error(msg) \
	do { perror(msg); exit(EXIT_FAILURE); } while (0)

int main(int argc, char *argv[])
{
	int efd, j;
	uint64_t u;
	ssize_t s;

	if (argc < 2) {
		fprintf(stderr, "Usage: %s <num>...\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	efd = eventfd(0, 0);
	if (efd == -1)
		handle_error("eventfd");

	switch (fork()) {
		case 0:
			for (j = 1; j < argc; j++) {
				sleep(1);
				printf("Child writing %s to efd\n", argv[j]);
				u = strtoull(argv[j], NULL, 0);
				/* strtoull() allows various bases */
				s = write(efd, &u, sizeof(uint64_t));
				if (s != sizeof(uint64_t))
					handle_error("write");
			}
			printf("Child completed write loop\n");

			exit(EXIT_SUCCESS);

		default:
			while (1) {
				printf("Parent about to read\n");
				s = read(efd, &u, sizeof(uint64_t));
				if (s != sizeof(uint64_t))
					handle_error("read");
				printf("Parent read %llu (0x%llx) from efd\n",
						(unsigned long long) u, (unsigned long long) u);
			}
			exit(EXIT_SUCCESS);

		case -1:
			handle_error("fork");
	}
}
