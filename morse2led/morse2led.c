#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <time.h>

#define OFF		"0"
#define BUFLEN		(16)
#define PATHLEN		(1024)

static inline int led(const char *onoff, FILE *fp)
{
	int r;

	r = fputs(onoff, fp);
	fflush(fp);

	return r;
}

#define NSEC2MSEC	(1000 * 1000)
static inline int morse_wait(int n)
{
	int r;
	struct timespec req = {
		0, n * 300 * NSEC2MSEC
	};

	if (req.tv_nsec >= 1000 * NSEC2MSEC) {
		req.tv_sec += req.tv_nsec / (1000 * NSEC2MSEC);
		req.tv_nsec %= 1000 * NSEC2MSEC;
	}
	r = nanosleep(&req, NULL);
	if (r < 0) {
		perror("nanosleep");
		return -1;
	}
	return 0;
}

#define SYSFS_LED_PATH_PREFIX	"/sys/class/leds/"
/* look for a sysfs file to control the caps led. */
static int look_for_led(char *buf, size_t bufsiz, const char *target)
{
	char *path = SYSFS_LED_PATH_PREFIX;
	struct dirent *dent;
	DIR *dir;

	dir = opendir(path);
	while ((dent = readdir(dir)) != NULL) {
		if (target == NULL) {
			if (dent->d_name[0] != '.')
				printf(" %s\n", dent->d_name);
		} else if (strstr(dent->d_name, target)) {
			int r;

			r = snprintf(buf, bufsiz, "%s%s",
				     path, dent->d_name);
			return r;
		}
	}
	closedir(dir);

	return 0;
}

int main(int argc, char *argv[])
{
	FILE *fp;
	int c, r;
	char ledsysfile[PATHLEN];
	char onstr[BUFLEN];
	const char *offstr = OFF;

	if (argc <= 1) {
		printf("Available sysfs entries for LED control:\n");
		look_for_led(ledsysfile, PATHLEN, NULL);
		return 0;
	}

	r = look_for_led(ledsysfile, PATHLEN, argv[1]);
	if (!r) {
		fprintf(stderr, "sysfs entry for led not found.\n");
		return -1;
	}

	printf("sysfs entry = %s\n", ledsysfile);

	/* max */
	strncat(ledsysfile, "/max_brightness", PATHLEN);
	fp = fopen(ledsysfile, "r");
	if (fp == NULL) {
		perror(ledsysfile);
		return -1;
	}
	if (fgets(onstr, BUFLEN, fp) == NULL) {
		perror(ledsysfile);
		return -1;
	}
	fclose(fp);

	ledsysfile[r] = '\0'; /* remove '/max_brightness' */
	strncat(ledsysfile, "/brightness", PATHLEN);
	fp = fopen(ledsysfile, "w+");
	if (fp == NULL) {
		perror(ledsysfile);
		return -1;
	}

	while ((c = getchar()) != EOF) {
		led(offstr, fp);
		morse_wait(1);
		switch ((unsigned char)c) {
		case '.':
			led(onstr, fp);
			morse_wait(1);
			break;
		case '-':
			led(onstr, fp);
		case ' ':
			morse_wait(3);
		default:
			break;
		}
	}
	fclose(fp);

	return 0;
}
