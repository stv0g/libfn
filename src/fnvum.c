#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <termios.h>
#include <fcntl.h>
#include <complex.h>

/* third party libs */
#include "libfn.h"
#include <SDL/SDL.h>
#include <pulse/simple.h>
#include <pulse/error.h>
#include <pulse/gccmacro.h>
#include <fftw3.h>

#define BUFFER_SIZE 1024
#define SAMPLING_RATE 44100
#define SCREEN_WIDTH (BUFFER_SIZE/2+1)
#define SCREEN_HEIGHT (SCREEN_WIDTH / 2)
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

void show_spectrum(SDL_Surface * dst, complex * fft_data) {
	int x, p, start, end;
	double log_data[BUFFER_SIZE];

	SDL_Rect rect;
        Uint32 color = SDL_MapRGB(dst->format, 0xff, 0, 0);
       	Uint32 background = SDL_MapRGB(dst->format, 0, 0, 0);

        SDL_FillRect(dst, &dst->clip_rect, background);

	double ampl, db;
       	rect.w = 1;

	for (x = 0; x < BUFFER_SIZE/2+1; x++) {
		ampl = cabs(fft_data[x]);

		rect.x = x;
//		rect.h = (ampl/3000000) * SCREEN_HEIGHT;
		rect.h = 10 * log10(ampl/300000) * SCREEN_HEIGHT;
		rect.y = SCREEN_HEIGHT - rect.h;

	        SDL_FillRect(dst, &rect, color);
	}
       	SDL_Flip(dst);
}

void fade_level(int fd, float level) {
	struct remote_msg_fade_rgb_t fncmd;
	memset(&fncmd, 0, sizeof (struct remote_msg_t));

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
		.format = PA_SAMPLE_S16LE,
		.rate = SAMPLING_RATE,
		.channels = 1
	};


	pa_simple *s = NULL;
	SDL_Surface *screen = NULL;
	SDL_Event event;

	int error, counter = 0, fd = -1;
	uint32_t level;
	int16_t * pcm_data;
	complex * fft_data;
        fftw_plan fft_plan;

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

	/* init fftw & get buffers*/
	pcm_data = (int16_t *) malloc(BUFFER_SIZE);
        fft_data = (complex *) fftw_malloc(BUFFER_SIZE * sizeof (complex));
	fft_plan = fftw_plan_dft_1d(BUFFER_SIZE, fft_data, fft_data, FFTW_FORWARD, 0);

	/* Create the recording stream */
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
		if (pa_simple_read(s, pcm_data, BUFFER_SIZE, &error) < 0) {
			fprintf(stderr, __FILE__": pa_simple_read() failed: %s\n", pa_strerror(error));
			exit(-1);
		}

		/* analyse audio data */
		uint16_t index, max = 0;
		uint32_t sum = 0;
		float level;
		for (index = 0; index < BUFFER_SIZE; index++) {
			sum += abs(pcm_data[index]);
			if (abs(pcm_data[index]) > max) max = pcm_data[index];

			fft_data[index] = (double) pcm_data[index];
		}

		/* execute fftw plan */
		fftw_execute(fft_plan);

		level = (float) sum / (BUFFER_SIZE * 32767);

		show_spectrum(screen, (complex *) fft_data);
		//show_level(screen, level);
		//fade_level(fd, level);

		//printf("level: %f \tsum: %d\t max: %d\n", level, sum, max);
	}

	/* housekeeping */
	SDL_Quit();
	free(pcm_data);
	fftw_free(fft_data);
	fftw_destroy(fft_plan);
	fftw_cleanup();

	return 0;
}
