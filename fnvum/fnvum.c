#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <math.h>

#include "../libfn.h"
#include <termios.h>
#include <fcntl.h>

#include <SDL/SDL.h>

#include <pulse/simple.h>
#include <pulse/error.h>
#include <pulse/gccmacro.h>

#define BUFFER_SIZE 1024
#define SCREEN_WIDTH 500
#define SCREEN_HEIGHT (SCREEN_WIDTH / 20)
#define TITLE "fnordlicht vumeter"
#define DECAY 0.7

void show_level(SDL_Surface *dst, float level) {
	SDL_Rect rect = dst->clip_rect;;
	Uint32 color = SDL_MapRGB(dst->format, 0xff, 0, 0);
	Uint32 background = SDL_MapRGB(dst->format, 0, 0, 0);

	SDL_FillRect(dst, &rect, background);

	rect.w *= level;

	SDL_FillRect(dst, &rect, color);
	SDL_Flip(dst);
}

int main(int argc, char*argv[]) {
	/* The sample type to use */
	static const pa_sample_spec ss = {
		.format = PA_SAMPLE_S16LE,
		.rate = 44100,
		.channels = 1
	};

	pa_simple *s = NULL;
	SDL_Surface *screen = NULL;
	SDL_Event event;

	int error, counter = 0;
	uint32_t level;
	int16_t buffer[BUFFER_SIZE];

	/* init fnordlichts */
	struct remote_msg_fade_rgb_t fncmd;
	int fd = open("/dev/ttyUSB0", O_RDWR | O_NOCTTY);
	if (fd < 0) {
		perror("/dev/ttyUSB0");
		exit(-1);
	}

	memset(&fncmd, 0, sizeof (struct remote_msg_t));
	fn_init(fd);

	fncmd.address = 255;
	fncmd.cmd = REMOTE_CMD_FADE_RGB;
	fncmd.step = 25;
	fncmd.delay = 0;

	/* init screen & window */
	if(SDL_Init(SDL_INIT_VIDEO) < 0) {
		fprintf(stderr, "Unable to init SDL: %s\n", SDL_GetError());
		exit(-1);
	}

	SDL_WM_SetCaption(TITLE, NULL);
	screen = SDL_SetVideoMode(SCREEN_WIDTH, SCREEN_HEIGHT, 16, SDL_SWSURFACE|SDL_ANYFORMAT|SDL_RESIZABLE);
	if (screen == NULL) {
		fprintf(stderr, "Unable to set 640x480 video: %s\n", SDL_GetError());
		exit(-1);
	}

	/* Create the recording stream */
	if (!(s = pa_simple_new(NULL, TITLE, PA_STREAM_RECORD, NULL, "record", &ss, NULL, NULL, &error))) {
		fprintf(stderr, __FILE__": pa_simple_new() failed: %s\n", pa_strerror(error));
		exit(-1);
	}

	pa_simple_flush(s, &error); /* flush audio buffer */
	while (1) {
		counter++;
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT) {
				printf("Good bye!\n");
				exit(0);
			}
		}

		if (pa_simple_read(s, buffer, sizeof(buffer), &error) < 0) {
			fprintf(stderr, __FILE__": pa_simple_read() failed: %s\n", pa_strerror(error));
			exit(-1);
		}

		int16_t * pos, max = 0;
		uint32_t sum = 0;
		float level;
		for (pos = buffer; pos - buffer < BUFFER_SIZE; pos++) {
			sum += abs(*pos);
			if (abs(*pos) > max) max = abs(*pos);
		}
		level = (float) sum / (BUFFER_SIZE * 32767) * 1.7;
		if (level > 1) level = 1;

		if (counter % (3 * 44100 / 1024) == 0) {
			fn_sync(fd);
			pa_simple_flush(s, &error);
			printf("synced & flushed\n");
		}

		show_level(screen, level);
		fncmd.color.red = fncmd.color.green = fncmd.color.blue = (uint8_t) 255.0 * level;
		fn_send(fd, (struct remote_msg_t *) &fncmd);
		printf("level: %d (%f) \tsum: %d\t max: %d\n", fncmd.color.red, level, sum, max);
	}

	/* housekeeping */
	SDL_Quit();

}
