#include "fft.h"

void ditfft2(CFftView in, FftView out)
{
    using namespace std::complex_literals;
    using namespace std::numbers;

    if (in.size() == 1) {
        out[0] = in[0];
        return;
    }

    const std::size_t N = in.size();
    ditfft2(in(0, N / 2, 2), out(0, N / 2));
    ditfft2(in(1, N / 2, 2), out(N / 2, N / 2));

    for (std::size_t k = 0; k < N / 2; k++) {
        std::complex<float> p = out[k];
        std::complex<float> q =
            std::exp((-2.0if * pi_v<float> * float(k)) / float(N)) * out[k + N / 2];
        out[k] = p + q;
        out[k + N / 2] = p - q;
    }
}
