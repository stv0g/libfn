/**
 * fnordlicht vu meter
 *
 * vizualize pulsaudio
 * @see http://www.steffenvogel.de/2010/11/12/fnordlicht-vu-meter/
 *
 * @copyright	2013 Steffen Vogel
 * @license	http://www.gnu.org/licenses/gpl.txt GNU Public License
 * @author	Steffen Vogel <post@steffenvogel.de>
 * @link	http://www.steffenvogel.de
 */
/*
 * This file is part of libfn
 *
 * libfn is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * libfn is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with libfn. If not, see <http://www.gnu.org/licenses/>.
 */

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

#include "libfn.h"

#define N		2048
#define SAMPLING_RATE	44100

#define MIN_FREQ	70
#define MAX_FREQ	9000

#define MIN_K		(MIN_FREQ*N/SAMPLING_RATE)
#define MAX_K		(MAX_FREQ*N /SAMPLING_RATE)

#define LINE_WIDTH	1
#define SCREEN_WIDTH	LINE_WIDTH*(MAX_K - MIN_K)
#define SCREEN_HEIGHT	SCREEN_WIDTH/2
#define VUM_HEIGHT	SCREEN_HEIGHT/6

#define TITLE		"fnordlicht visualization"

double normalize_auditory(double freq, double spl) {
	return spl; // TODO implement
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

struct rgb_color_t phasor2color(complex phasor) {
	struct hsv_color_t hsv;

	hsv.hue = 180*(carg(phasor)+M_PI)/M_PI;
	hsv.saturation = 255;
	hsv.value = 255;

	return hsv2rgb(hsv);
}

struct rgb_color_t level2color(double level) {
	struct hsv_color_t hsv;

	hsv.hue = level * 360;
	hsv.saturation = 255;
	hsv.value = 255;

	return hsv2rgb(hsv);
}

void show_spectrum(SDL_Surface * dst, complex * fft_data) {
	struct rgb_color_t rgb;
	uint32_t background = SDL_MapRGB(dst->format, 0, 0, 0);
	uint32_t foreground;
	SDL_FillRect(dst, &dst->clip_rect, background);

	SDL_Rect rect;
	rect.w = LINE_WIDTH;

	int k;
	for (k = MIN_K; k <= MAX_K; k++) {
		double ampl = cabs(fft_data[k]) / 500000;
		rect.x = (k - MIN_K) * LINE_WIDTH;
		rect.h = ampl * (SCREEN_HEIGHT - VUM_HEIGHT);
		rect.y = (SCREEN_HEIGHT - VUM_HEIGHT) - rect.h;

		rgb = phasor2color(fft_data[k]);
		foreground = SDL_MapRGB(dst->format, rgb.red, rgb.green, rgb.blue);

	        SDL_FillRect(dst, &rect, foreground);
	}
}

void fade_spectrum(int fd, complex * fft_data, int fn_num) {
	struct remote_msg_t fn_cmd;
	memset(&fn_cmd, 0, sizeof (struct remote_msg_t));

	fn_cmd.cmd = REMOTE_CMD_FADE_RGB;
	fn_cmd.fade_rgb.step = 200;
	fn_cmd.fade_rgb.delay = 0;

	int k;
	for (k = 0; k < fn_num; k++) {
		double ampl = 0;
		int i;
		for (i = MIN_K+k*(MAX_K-MIN_K)/fn_num; i < (k+1)*(MAX_K-MIN_K)/fn_num; i++) {
			ampl += cabs(fft_data[i]);
		}
		ampl /= 1000000*fn_num;

		fn_cmd.fade_rgb.color.red = 255 * ampl;
		fn_cmd.fade_rgb.color.green = 255 * ampl;
		fn_cmd.fade_rgb.color.blue = 255 * ampl;

		fn_cmd.address = k;
		fn_send(fd, &fn_cmd);
	}
}

void show_level(SDL_Surface *dst, float level) {
	SDL_Rect rect = {0, SCREEN_HEIGHT - VUM_HEIGHT, SCREEN_WIDTH, VUM_HEIGHT};

	struct rgb_color_t rgb = level2color(level);
	uint32_t background = SDL_MapRGB(dst->format, 0, 0, 0);
	uint32_t foreground = SDL_MapRGB(dst->format, rgb.red, rgb.green, rgb.blue);

	SDL_FillRect(dst, &rect, background);
	rect.w *= level;
	SDL_FillRect(dst, &rect, foreground);
}

void fade_level(int fd, double level) {
	struct remote_msg_t fn_cmd;
//	struct rgb_color_t rgb = {{{255, 255, 255}}};
	struct rgb_color_t rgb = level2color(level);
	memset(&fn_cmd, 0, sizeof(struct remote_msg_t));

	fn_cmd.cmd = REMOTE_CMD_FADE_RGB;
	fn_cmd.address = 255;

	fn_cmd.fade_rgb.step = 60;
	fn_cmd.fade_rgb.delay = 2;

	fn_cmd.fade_rgb.color.red = rgb.red * level;
	fn_cmd.fade_rgb.color.green = rgb.green * level;
	fn_cmd.fade_rgb.color.blue = rgb.blue * level;

	fn_send(fd, &fn_cmd);
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

	int error, counter = 0, fd = -1, fn_num;
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
		fn_sync(fd);

		if (argc > 2) {
			fn_num = atoi(argv[2]);
			printf("set to %d fnordlichts\n", fn_num);
		}
		else {
			fn_num = fn_count_devices(fd);
			printf("found %d fnordlichts\n", fn_num);
			usleep(25000);
		}
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
		
		/* execute fftw plan */
		fftw_execute(fft_plan);
		level = (float) sum / (N * pow(2, 15)) * 2;
		
		//if (counter % 2 == 0) fade_spectrum(fd, fft_data, fn_num);
		if (level > 0.67) fade_level(fd, 1);
		else fade_level(fd, 0);
		//if (level > 0.05) fade_level(fd, level);

		show_spectrum(screen, fft_data);
		show_level(screen, level);		
		SDL_Flip(screen);

		//printf("level: %f \tsum: %d\t max: %d\n", level, sum, max);
	}

	/* housekeeping */
	SDL_Quit();
	free(pcm_data);
	fftw_free(fft_data);
	fftw_cleanup();

	return 0;
}
