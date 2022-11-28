#include "fftseq.h"
#include <numbers>

void bswap2(char* ptr)
{
    char tmp = ptr[0];
    ptr[0] = ptr[1];
    ptr[1] = tmp;
}

void bswap4(char* ptr)
{
    char tmp0 = ptr[0];
    char tmp1 = ptr[1];
    ptr[0] = ptr[3];
    ptr[1] = ptr[2];
    ptr[2] = tmp1;
    ptr[3] = tmp0;
}

std::vector<float> blackman(std::size_t N)
{
    using namespace std::numbers;

    static const float a0 = 0.42f;
    static const float a1 = 0.50f;
    static const float a2 = 0.50f;

    std::vector<float> win(N);

    for (std::size_t n = 1; n < N + 1; ++n) {
        win[n - 1] = a0 - a1 * std::cos((2.0f * pi_v<float> * n) / N)
            + a2 * std::cos((4.0f * pi_v<float> * n) / N);
    }

    return win;
}

std::vector<float> rectangular(std::size_t N)
{
    return std::vector<float>(N, 1.0f);
}

FftSeq::FftSeq(Stream& stream, std::size_t fft_size, const WinFn& win_fn)
    : _stream(stream), _fft_size(fft_size), _window_fn(win_fn), _done(false) {}

FftSeq::~FftSeq()
{
    _quit = true;
    _done = false;
    _done.notify_one();
    if (_worker.joinable()) {
        _worker.join();
    }
}

void FftSeq::start()
{
    _worker = std::thread(&FftSeq::worker_fn, this);
}

std::size_t FftSeq::fft_size() const
{
    return _fft_size;
}

/*
void FftSeq::fft_size(std::size_t size)
{
    _fft_size = size;
}
*/

void FftSeq::spacing(int spacing)
{
    _spacing = spacing;
}

int FftSeq::spacing() const
{
    return _spacing;
}

void FftSeq::optimal_spacing(float srate, float fft_rate)
{
    float samples_per_fft = srate / fft_rate;
    _spacing = (int) (0.5 + samples_per_fft - _fft_size);
}

bool FftSeq::has_next() const
{
    return _done.load();
}

std::vector<std::complex<float>>&& FftSeq::next()
{
    return std::move(_result);
}

void FftSeq::notify()
{
    _done = false;
    _done.notify_one();
}

void FftSeq::worker_fn()
{
    std::size_t size = 0;
    std::vector<float> window;
    std::vector<std::complex<float>> buffer;
    while (1) {
        if (size != _fft_size) {
            size = _fft_size;
            window = _window_fn(size);

            if (_spacing < 0) {
                buffer = std::vector<std::complex<float>>(size);
            }
        }

        std::vector<std::complex<float>> in_vec;

        if (_spacing >= 0) {
            _stream.skip(_spacing);
            in_vec = _stream.read_chunk(size);
        } else if (_spacing < 0) {
            std::move(buffer.end() + _spacing, buffer.end(), buffer.begin());
            auto tmp = _stream.read_chunk(size + _spacing);
            std::copy_n(tmp.begin(), tmp.size(), buffer.begin());
            in_vec = buffer;
        }

        for (std::size_t i = 0; i < size; i++) {
            in_vec[i] *= window[i];
        }

        _result.resize(size);
        ditfft2(in_vec, _result);

        if (_quit) {
            break;
        }

        _done = true;
        _done.wait(true);
    }
}
