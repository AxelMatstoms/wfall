#ifndef WFALL_FFT_H
#define WFALL_FFT_H

#include <vector>
#include <complex>
#include <algorithm>

template <bool IsConst>
struct fft_view_container {};

template <> struct fft_view_container<false> {
    using type = std::vector<std::complex<float>>&;
};

template <> struct fft_view_container<true> {
    using type = const std::vector<std::complex<float>>&;
};

template <bool IsConst>
using fft_view_container_t = fft_view_container<IsConst>::type;

template <bool IsConst>
class FftViewTemplate {
public:
    using reference = std::complex<float>&;

    using const_reference = const std::complex<float>&;

    using container = fft_view_container_t<IsConst>;

private:
    container _vec;
    std::size_t _start = 0;
    std::size_t _size;
    std::size_t _stride = 1;

public:
    FftViewTemplate(container vec) : _vec(vec), _size(vec.size()) {}
    FftViewTemplate(container vec, std::size_t start, std::size_t size, std::size_t stride)
        : _vec(vec), _start(start), _size(size), _stride(stride) {}

    std::size_t size() const { return _size; }

    reference operator[](std::size_t pos) requires (!IsConst)
    {
        return _vec[_start + _stride * pos];
    }

    const_reference operator[](std::size_t pos) const
    {
        return _vec[_start + _stride * pos];
    }

    FftViewTemplate operator()(std::size_t start, std::size_t size, std::size_t stride = 1) const requires IsConst
    {
        return FftViewTemplate(_vec, _start + start * _stride, size, _stride * stride);
    }

    FftViewTemplate operator()(std::size_t start, std::size_t size, std::size_t stride = 1) requires (!IsConst)
    {
        return FftViewTemplate(_vec, _start + start * _stride, size, _stride * stride);
    }
};

using CFftView = FftViewTemplate<true>;
using FftView = FftViewTemplate<false>;

void ditfft2(CFftView in, FftView out);

#endif /* WFALL_FFT_H */
