#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

/* third party libs */
#include "../lib/libfn.h"
#include "../lib/dft.h"

#include <SDL/SDL.h>
#include <pulse/simple.h>
#include <pulse/error.h>
#include <pulse/gccmacro.h>

#define BUFFER_SIZE 1024
#define SAMPLING_RATE 44100
#define SCREEN_WIDTH (BUFFER_SIZE/2)
#define SCREEN_HEIGHT (SCREEN_WIDTH/2)
#define TITLE "fnordlicht vumeter & spectrum"

void show_level(SDL_Surface *dst, float level) {
	SDL_Rect rect = dst->clip_rect;
	Uint32 color = SDL_MapRGB(dst->format, 0xff, 0, 0);
	Uint32 background = SDL_MapRGB(dst->format, 0, 0, 0);

	SDL_FillRect(dst, &rect, background);

	rect.w *= level;

	SDL_FillRect(dst, &rect, color);
	SDL_Flip(dst);
}

void show_spectrum(SDL_Surface * dst, complex_t * dft_data) {
	int k;

	SDL_Rect rect;
        Uint32 color = SDL_MapRGB(dst->format, 0xff, 0, 0);
       	Uint32 background = SDL_MapRGB(dst->format, 0, 0, 0);

        SDL_FillRect(dst, &dst->clip_rect, background);

	double ampl;
       	rect.w = 1;

	for (k = 0; k < SCREEN_WIDTH; k++) {
		ampl = complex_abs(dft_data[k]);
		
		rect.x = k;
		rect.h = (ampl / 100000) * SCREEN_HEIGHT;
		rect.y = SCREEN_HEIGHT - rect.h;

	        SDL_FillRect(dst, &rect, color);
	}
       	SDL_Flip(dst);
}

void fade_level(int fd, double level) {
	struct remote_msg_fade_rgb_t fncmd;
	memset(&fncmd, 0, sizeof(struct remote_msg_t));

	fncmd.color.red = fncmd.color.green = fncmd.color.blue = (uint8_t) 255.0 * level;
	fncmd.address = 255;
	fncmd.cmd = REMOTE_CMD_FADE_RGB;
	fncmd.step = 25;
	fncmd.delay = 0;

	fn_send(fd, (struct remote_msg_t *) &fncmd);
}

int main(int argc, char *argv[]) {
	/* The sample type to use */
	static const pa_sample_spec ss = {
		.format = PA_SAMPLE_S16NE,
		.rate = SAMPLING_RATE,
		.channels = 1
	};

	pa_simple *s = NULL;
	SDL_Surface *screen = NULL;
	SDL_Event event;

	int error, counter = 0, fd = -1;
	int16_t * pcm_data;
	complex_t * dft_data;

	/* init fnordlichts */
	if (argc > 1) {
		fd = open(argv[1], O_RDWR | O_NOCTTY);
		if (fd < 0) {
			perror(argv[0]);
			exit(-1);
		}

		fn_init(fd);
	}

	/* init screen & window */
	if(SDL_Init(SDL_INIT_VIDEO) < 0) {
		fprintf(stderr, "Unable to init SDL: %s\n", SDL_GetError());
		exit(-1);
	}

	/* open sdl window */
	SDL_WM_SetCaption(TITLE, NULL);
	screen = SDL_SetVideoMode(SCREEN_WIDTH, SCREEN_HEIGHT, 16, SDL_SWSURFACE|SDL_ANYFORMAT|SDL_RESIZABLE);
	if (screen == NULL) {
		fprintf(stderr, "Unable to set video: %s\n", SDL_GetError());
		exit(-1);
	}

	/* allocate buffers */
	pcm_data = malloc(BUFFER_SIZE * sizeof(int16_t));
	dft_data = malloc(BUFFER_SIZE * sizeof(complex_t));

	/* open the recording stream */
	if (!(s = pa_simple_new(NULL, TITLE, PA_STREAM_RECORD, NULL, "record", &ss, NULL, NULL, &error))) {
		fprintf(stderr, __FILE__": pa_simple_new() failed: %s\n", pa_strerror(error));
		exit(-1);
	}
	pa_simple_flush(s, &error); /* flush audio buffer */

	while (1) {
		counter++;

		/* handle SDL events */
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT) {
				printf("Good bye!\n");
				exit(0);
			}
		}

		/* read PCM audio data */
		if (pa_simple_read(s, pcm_data, sizeof(pcm_data[0]) * BUFFER_SIZE, &error) < 0) {
			fprintf(stderr, __FILE__": pa_simple_read() failed: %s\n", pa_strerror(error));
			exit(-1);
		}

		/* do the dft and show spectrum */
		dft(pcm_data, dft_data, BUFFER_SIZE);
		show_spectrum(screen, dft_data);
		
		/*int i;
		for (i = 0; i < BUFFER_SIZE; i++) {
			printf("%d ", pcm_data[i]);
		}
		printf("\n\n");*/
	}

	/* housekeeping */
	SDL_Quit();

	return 0;
}
