#include <math.h>
#include <stdint.h>
#include <string.h>

#include "dft.h"

double complex_abs(complex_t z) {
	return sqrt(z.imag * z.imag + z.real * z.real);
}

complex_t * dft(int16_t * in, complex_t * out, int16_t n) {
	uint16_t  k, l;

	// clear output
	memset(out, 0, n * sizeof(complex_t));

	for (k = 0; k < n/2; k++) {
		for (l = 0; l < n; l++) {
			double phasor = -2*M_PI*k*l/n;
			
			out[k].real += in[l] * cos(phasor);
			out[k].imag += in[l] * sin(phasor);
		}
	}

	return out;
}
