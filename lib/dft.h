#ifndef LIBDFT_H
#define LIBDFT_H

typedef struct complex {
	double real;
	double imag;
} complex_t;

double complex_abs(complex_t z);
complex_t * dft(int16_t * in, complex_t * out, int16_t n);

#endif
