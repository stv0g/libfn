#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <math.h>

#include <pulse/simple.h>
#include <pulse/error.h>
#include <pulse/gccmacro.h>

#define BUFFER_SIZE 44100
#define DECAY 0.7

int main(int argc, char*argv[]) {
	/* The sample type to use */
	static const pa_sample_spec ss = {
		.format = PA_SAMPLE_S16LE,
		.rate = 44100,
		.channels = 1
	};

	pa_simple *s = NULL;
	int error;
	uint32_t level;
	int16_t buffer[BUFFER_SIZE];


	/* Create the recording stream */
	if (!(s = pa_simple_new(NULL, "fnordlicht VU Meter", PA_STREAM_RECORD, NULL, "record", &ss, NULL, NULL, &error))) {
		fprintf(stderr, __FILE__": pa_simple_new() failed: %s\n", pa_strerror(error));
		exit(-1);
	}

	pa_simple_flush(s, &error); /* flush audio buffer */
	for (;;) {
		if (pa_simple_read(s, buffer, sizeof(buffer), &error) < 0) {
			fprintf(stderr, __FILE__": pa_simple_read() failed: %s\n", pa_strerror(error));
			exit(-1);
		}

		int16_t * pos, max = 0;
		uint32_t sum = 0;
		for (pos = buffer; pos - buffer < BUFFER_SIZE; pos++) {
			sum += abs(*pos);
			if (abs(*pos) > max) max = abs(*pos);
//			printf("%d\n", abs(*pos));
		}
		printf("max: %d\nsum: %d\n", max, sum);
	}
}
