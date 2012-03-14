#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "math.h"
#include "time.h"

double deg2rad(double deg) {
	return M_PI * deg / 180.0;
}

enum sun { RISE, SET };
enum out { HUMAN, UNIX };

double sun(enum sun mode, double lat, double lon, int timezone) {
	const double h = -0.0145;
	const double g = 0.2618;

	time_t t = time(NULL);
	struct tm *now = localtime(&t);
	int days = now->tm_yday;

	/* Zeitgleichung */
	double zgl = -0.171 * sin(0.0337 * days + 0.465) - 0.1299 * sin(0.01787 * days - 0.168) ;

	/* Deklination der Sonne */
	double dekl = 0.4095 * sin(0.016906 * (days - 80.086));

	/* Zeitdifferenz */
	double zd = 12 * acos((sin(h) - sin(lat) * sin(dekl)) / (cos(lat) * cos(dekl))) / M_PI;

	switch (mode) {
		case SET: return 12 + zd - zgl + lon/g + timezone;
		case RISE: return 12 - zd - zgl + lon/g + timezone;
		default: return 0;
	}
}

int main(int argc, char *argv[]) {
	enum sun mode;
	enum out output;

	if (argc < 4 || argc > 6) goto usage;


	if (strcmp(argv[1], "rise") == 0) mode = RISE;
	else if (strcmp(argv[1], "set") == 0) mode = SET;
	else goto usage;

	if (argc >= 6) {
		if (strcmp(argv[5], "human") == 0) output = HUMAN;
		else if (strcmp(argv[5], "unix") == 0) output = UNIX;
		else goto usage;
	}
	else {
		output = HUMAN;
	}

	int timezone = (argc >= 5) ? atoi(argv[4]) : 0;
	double lat = deg2rad(strtod(argv[2], NULL));
	double lon = deg2rad(strtod(argv[3], NULL));
	double time = sun(mode, lat, lon, timezone);
	double intpart;

	switch (output) {
		case HUMAN:
			printf("%d:%02d\n", (int) floor(time), (int) (modf(time, &intpart)*60));
			break;
		case UNIX:
			printf("%d\n", (int) round(time*60*60));
			break;
	}

	return 0;

usage:
	fprintf(stderr, "usage: sun MODE LAT LON [TIMEZONE] [OUTPUT]\n");
	fprintf(stderr, "  MODE\t\t'rise' or 'set'\n");
	fprintf(stderr, "  LAT, LON\tthe geografical postition in degree\n");
	fprintf(stderr, "  TIMEZONE\tan integer describing the timezone offset\n");
	fprintf(stderr, "  OUTPUT\t'human' (default) or 'unix'\n");

	return -1;
}
