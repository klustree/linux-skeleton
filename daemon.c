#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <fcntl.h>
#include <errno.h>

#define DEBUG(fmt, args...) fprintf(stderr, "DEBUG: " fmt, ##args)
#define ERROR(fmt, args...) fprintf(stderr, "ERROR: " fmt, ##args)
#define PANIC(fmt, args...) \
({ 							\
 	fprintf(stderr, "PANIC: " fmt, ##args);	\
	abort(); 				\
})

const char *program_name = "deamon";
char *pid_file = NULL;
static bool is_daemon = true;

static int create_pidfile(const char *filename)
{
#define RUN_PATH "/var/run"
	int fd;
	int len;
	char buffer[32];

	fd = open(filename, O_RDWR|O_CREAT|O_SYNC, 0600);
	if (fd < 0)
		return -1;

	if (lockf(fd, F_TLOCK, 0) == -1) {
		close(fd);
		return -1;
	}

	len = snprintf(buffer, sizeof(buffer), "%d\n", getpid());
	if (write(fd, buffer, len) != len) {
		close(fd);
		return -1;
	}

	/* keep pidfile open & locked forever */
	return 0;
}

static int create_lockfile(const char *filename)
{
#define LOCK_PATH "/var/lock"
	int fd;
	char *lock_path;
	int ret = 0;
	int len = strlen(filename) + strlen(LOCK_PATH) + 1;

	lock_path = calloc(1, len);
	snprintf(lock_path, len + 1, "%s/%s", LOCK_PATH, filename);

	fd = open(lock_path, O_WRONLY|O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
	if (fd < 0) {
		err("failed to open lock file %s (%m)", lock_path);
		goto out;
	}

	if (lockf(fd, F_TLOCK, 1) < 0) {
		if (errno == EACCES || errno == EAGAIN)
			ERROR("daemon is already running %s", lock_path);
		else
			ERROR("unable to get daemon lock (%m)");
		return -1;
		goto out;
	}

out:
	free(lock_path);
	return ret;
}

static int lock_and_daemon(bool daemonize, const char *lockfile)
{
	int ret, status = 0;
	int pipefd[2];

	ret = pipe(pipefd);
	if (ret < 0)
		PANIC("pipe() for passing exit status failed: %m");

	if (daemonize) {
		switch (fork()) {
			case 0:
				break;
			case -1:
				PANIC("fork() failed during daemonize: %m");
				break;
			default:
				ret = read(pipefd[0], &status, sizeof(status));
				if (ret != sizeof(status))
					PANIC("read exit status failed: %m");

				exit(status);
				break;
		}

		if (setsid() < 0) {
			ERROR("becoming a leader of a new session failed: %m");
			status = -1;
			goto out;
		}

		if (chdir("/")) {
			ERROR("chdir to / failed: %m");
			status = -1;
			goto out;
		}
	}

	ret = create_lockfile(lockfile);
	if (ret < 0) {
		ERROR("locking program: %s failed", program_name);
		status = -1;
		goto out;
	}

	if (daemonize) {
		close(STDIN_FILENO);
		close(STDOUT_FILENO);
		close(STDERR_FILENO);
	}

out:
	ret = write(pipefd[1], &status, sizeof(status));
	if (ret != sizeof(status))
		PANIC("writing exit status failed: %m");

	return status;
}

int main(int argc, char *argv[])
{
	if (lock_and_daemon(is_daemon, "daemon.lock") < 0)
		exit(1);

	while (1)
	{
		DEBUG("daemon running");
		sleep(5);
	}

	if (pid_file)
		unlink(pid_file);
}
