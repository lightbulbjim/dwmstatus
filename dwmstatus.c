#define _BSD_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <X11/Xlib.h>

char *tz = "Australia/Sydney";

static Display *dpy;

char *
smprintf(char *fmt, ...)
{
	va_list fmtargs;
	char *ret;
	int len;

	va_start(fmtargs, fmt);
	len = vsnprintf(NULL, 0, fmt, fmtargs);
	va_end(fmtargs);

	ret = malloc(++len);
	if (ret == NULL) {
		perror("malloc");
		exit(1);
	}

	va_start(fmtargs, fmt);
	vsnprintf(ret, len, fmt, fmtargs);
	va_end(fmtargs);

	return ret;
}

void
settz(char *tzname)
{
	setenv("TZ", tzname, 1);
}

char *
mktimes(char *fmt, char *tzname)
{
	char buf[129];
	time_t tim;
	struct tm *timtm;

	bzero(buf, sizeof(buf));
	settz(tzname);
	tim = time(NULL);
	timtm = localtime(&tim);
	if (timtm == NULL) {
		perror("localtime");
		exit(1);
	}

	if (!strftime(buf, sizeof(buf)-1, fmt, timtm)) {
		fprintf(stderr, "strftime == 0\n");
		exit(1);
	}

	return smprintf("%s", buf);
}

void
setstatus(char *str)
{
	XStoreName(dpy, DefaultRootWindow(dpy), str);
	XSync(dpy, False);
}

char *
loadavg(void)
{
	double avgs[3];

	if (getloadavg(avgs, 3) < 0) {
		perror("getloadavg");
		exit(1);
	}

	return smprintf("%.2f", avgs[1]);
}

char *
getmail()
{
	char *path;
	FILE *fd;
	path = "/tmp/.mail";
	fd = fopen(path, "r");
	if (fd == NULL) {
		return "";
	}

	long size;
	fseek(fd, 0, SEEK_END);
	size = ftell(fd);
	if (size == 0) {
		fclose(fd);
		return "";
	}

	char *line = malloc(8);
	fseek(fd, 0, SEEK_SET);
	fgets(line, strlen(line)-1, fd);
	char *c = strchr(line, '\n');
	if (c) {
		*c = '\0';
	}
	fclose(fd);
	return line;
}

char *
getbattery(char *base)
{
	char *path, line[513];
	FILE *fd;
	int descap, remcap, disrate;

	descap = -1;
	remcap = -1;
	disrate = -1;

	path = smprintf("%s/info", base);
	fd = fopen(path, "r");
	if (fd == NULL) {
		perror("fopen");
		exit(1);
	}
	free(path);
	while (!feof(fd)) {
		if (fgets(line, sizeof(line)-1, fd) == NULL)
			break;

		if (!strncmp(line, "present", 7)) {
			if (strstr(line, " no")) {
				descap = 1;
				break;
			}
		}
		if (!strncmp(line, "design capacity", 15)) {
			if (sscanf(line+16, "%*[ ]%d%*[^\n]", &descap))
				break;
		}
	}
	fclose(fd);

	path = smprintf("%s/state", base);
	fd = fopen(path, "r");
	if (fd == NULL) {
		perror("fopen");
		exit(1);
	}
	free(path);
	while (!feof(fd)) {
		if (fgets(line, sizeof(line)-1, fd) == NULL)
			break;

		if (!strncmp(line, "present", 7)) {
			if (strstr(line, " no")) {
				remcap = 1;
				disrate = 0;
				break;
			}
		}
		if (!strncmp(line, "remaining capacity", 18))
			sscanf(line+19, "%*[ ]%d%*[^\n]", &remcap);
		if (!strncmp(line, "present rate", 12))
			sscanf(line+13, "%*[ ]%d%*[^\n]", &disrate);
	}
	fclose(fd);

	if (remcap < 0 || descap < 0)
		return NULL;

	return smprintf("%dmW %.0f%%", disrate, ((float)remcap / (float)descap) * 100);
}

int
main(void)
{
	char *status;
	char *avgs;
	char *bat;
	char *mail;
	char *tm;

	if (!(dpy = XOpenDisplay(NULL))) {
		fprintf(stderr, "dwmstatus: cannot open display.\n");
		return 1;
	}

	for (;;sleep(1)) {
		avgs = loadavg();
		bat = getbattery("/proc/acpi/battery/BAT1");
		mail = getmail();
		tm = mktimes("%a %b %e %H:%M:%S", tz);

		if (strlen(mail) > 0) {
			status = smprintf("%s | %s | %s | %s",
					tm, mail, bat, avgs);
			free(mail);
		} else {
			status = smprintf("%s | %s | %s",
					tm, bat, avgs);
		}
		setstatus(status);
		free(avgs);
		free(bat);
		free(tm);
		free(status);
	}

	XCloseDisplay(dpy);

	return 0;
}

