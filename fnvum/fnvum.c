#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <pulse/simple.h>
#include <pulse/error.h>
#include <pulse/gccmacro.h>

#define BUFSIZE 1024

static ssize_t loop_write(int fd, const void*data, size_t size) {
    ssize_t ret = 0;

    while (size > 0) {
        ssize_t r;

        if ((r = write(fd, data, size)) < 0)
            return r;

        if (r == 0)
            break;

        ret += r;
        data = (const uint8_t*) data + r;
        size -= (size_t) r;
    }

    return ret;
}


int main(int argc, char*argv[]) {
    /* The sample type to use */
    static const pa_sample_spec ss = {
	.format = PA_SAMPLE_S16LE,
        .rate = 44100,
        .channels = 1
    };
    pa_simple *s = NULL;
    int error;

    /* Create the recording stream */
    if (!(s = pa_simple_new(NULL, "fnordlicht VU Meter", PA_STREAM_RECORD, NULL, "record", &ss, NULL, NULL, &error))) {
        fprintf(stderr, __FILE__": pa_simple_new() failed: %s\n", pa_strerror(error));
        exit(-1);
    }

	for (;;) {
		uint32_t level = 0;

		for (int i = 0;i < 40;i++) {
			int16_t buf[BUFSIZE];

			/* Record some data ... */
			if (pa_simple_read(s, buf, sizeof(buf), &error) < 0) {
				fprintf(stderr, __FILE__": pa_simple_read() failed: %s\n", pa_strerror(error));
				exit(-1);
			}

/*			for (int i = 0; i < sizeof(buf); i++) {
				int16_t sample = *(buf+i);

				if (sample < 0)
					sample *= -1;

				level += sample;
			}*/

if (loop_write(STDOUT_FILENO, buf, sizeof(buf)) != sizeof(buf)) {
            fprintf(stderr, __FILE__": write() failed: %s\n", strerror(errno));
	exit(-1);
        }


		}

//		printf("%i\n", level);
	}
}
