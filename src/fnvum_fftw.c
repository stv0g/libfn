#include "../lib/libfn.h"

/* stdlibs */
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
#include <SDL/SDL.h>
#include <pulse/simple.h>
#include <pulse/error.h>
#include <pulse/gccmacro.h>
#include <fftw3.h>

#define N		512
#define SAMPLING_RATE	44100

#define MIN_FREQ	50
#define MAX_FREQ	9000

#define MIN_K		(MIN_FREQ*N/SAMPLING_RATE)+1
#define MAX_K		(MAX_FREQ*N /SAMPLING_RATE)

#define LINE_WIDTH	4
#define SCREEN_WIDTH	LINE_WIDTH*(MAX_K - MIN_K)
#define SCREEN_HEIGHT	SCREEN_WIDTH/2

#define TITLE		"fnordlicht visualization"

double normalize_auditory(double freq, double spl) {
	return spl; // TODO implement
}

void show_level(SDL_Surface *dst, float level) {
	SDL_Rect rect = dst->clip_rect;
	uint32_t color = SDL_MapRGB(dst->format, 0xff, 0, 0); // TODO define via phase
	uint32_t background = SDL_MapRGB(dst->format, 0, 0, 0);

	SDL_FillRect(dst, &rect, background);

	rect.w *= level;

	SDL_FillRect(dst, &rect, color);
	SDL_Flip(dst);
}

struct rgb_color_t hsv2rgb(struct hsv_color_t hsv) {
	struct rgb_color_t rgb;

	uint16_t h = hsv.hue % 360;
	uint8_t s = hsv.saturation;
	uint8_t v = hsv.value;
    
	uint16_t f = ((h % 60) * 255 + 30)/60;
	uint16_t p = (v * (255-s)+128)/255;
	uint16_t q = ((v * (255 - (s*f+128)/255))+128)/255;
	uint16_t t = (v * (255 - ((s * (255 - f))/255)))/255;
	uint8_t i = h/60;

	switch (i) {
		case 0:	rgb.red = v; rgb.green = t; rgb.blue = p; break;
		case 1:	rgb.red = q; rgb.green = v; rgb.blue = p; break;
		case 2:	rgb.red = p; rgb.green = v; rgb.blue = t; break;
		case 3:	rgb.red = p; rgb.green = q; rgb.blue = v; break;
		case 4:	rgb.red = t; rgb.green = p; rgb.blue = v; break;
		case 5:	rgb.red = v; rgb.green = p; rgb.blue = q; break;
	}
	
	return rgb;
}

uint32_t phasor2color(complex phasor, SDL_PixelFormat *format) {
	struct hsv_color_t hsv;
	struct rgb_color_t rgb;
	
	hsv.hue = 180*(carg(phasor)+M_PI)/M_PI;
	hsv.saturation = 255;
	hsv.value = 255;
	
	rgb = hsv2rgb(hsv);

	return SDL_MapRGB(format, rgb.red, rgb.green, rgb.blue);
}

void show_spectrum(SDL_Surface * dst, complex * fft_data) {
	SDL_Rect rect;
	uint32_t background = SDL_MapRGB(dst->format, 0, 0, 0);

	SDL_FillRect(dst, &dst->clip_rect, background);
	
	rect.w = LINE_WIDTH;
       	
	int k;
	for (k = MIN_K; k <= MAX_K; k++) {
		double ampl = cabs(fft_data[k]);
		
		rect.x = (k - MIN_K + 1) * LINE_WIDTH;
		
		rect.h = (ampl / 900000) * SCREEN_HEIGHT;
		rect.y = SCREEN_HEIGHT - rect.h;

	        SDL_FillRect(dst, &rect, phasor2color(fft_data[k], dst->format));
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
	screen = SDL_SetVideoMode(SCREEN_WIDTH, SCREEN_HEIGHT, 16, SDL_SWSURFACE|SDL_ANYFORMAT);
	if (screen == NULL) {
		fprintf(stderr, "Unable to set video: %s\n", SDL_GetError());
		exit(-1);
	}

	/* init fftw & get buffers*/
	pcm_data = (int16_t *) malloc(N * sizeof (int16_t));
        fft_data = (complex *) fftw_malloc(N * sizeof (complex));
	fft_plan = fftw_plan_dft_1d(N, fft_data, fft_data, FFTW_FORWARD, 0);

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
		if (pa_simple_read(s, pcm_data, N * sizeof (int16_t), &error) < 0) {
			fprintf(stderr, __FILE__": pa_simple_read() failed: %s\n", pa_strerror(error));
			exit(-1);
		}

		/* analyse audio data */
		int16_t index, max = 0;
		uint32_t sum = 0;
		float level;
		for (index = 0; index < N; index++) {
			sum += abs(pcm_data[index]);
			if (abs(pcm_data[index]) > max) max = pcm_data[index];

			fft_data[index] = (double) pcm_data[index];
		}
		
		level = (float) sum / (N * pow(2, 15));
		//show_level(screen, level * 1.8);
		//fade_level(fd, level);

		/* execute fftw plan */
		fftw_execute(fft_plan);
		show_spectrum(screen, fft_data);

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
