#ifndef WFALL_FFT_H
#define WFALL_FFT_H

#include <vector>
#include <complex>

template <bool IsConst>
struct fft_slice_reference {};

template <> struct fft_slice_reference<false> {
    using type = std::complex<float>&;
};

template <> struct fft_slice_reference<true> {
    using type = std::complex<float>&;
};

template <bool IsConst>
using fft_slice_reference_t = fft_slice_reference<IsConst>::type;

template <bool IsConst>
struct fft_slice_container {};

template <> struct fft_slice_container<false> {
    using type = std::vector<std::complex<float>>&;
};

template <> struct fft_slice_container<true> {
    using type = const std::vector<std::complex<float>>&;
};

template <bool IsConst>
using fft_slice_container_t = fft_slice_container<IsConst>::type;

template <bool IsConst>
class FftSliceTemp {
public:
    using reference = fft_slice_reference_t<IsConst>;

    using const_reference = const std::complex<float>&;

    using container = fft_slice_container_t<IsConst>;

private:
    container _vec;
    std::size_t _start = 0;
    std::size_t _size;
    std::size_t _stride = 1;

public:
    FftSliceTemp(container vec) : _vec(vec), _size(vec.size()) {}
    FftSliceTemp(container vec, std::size_t start, std::size_t size, std::size_t stride)
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

    FftSliceTemp operator()(std::size_t start, std::size_t size, std::size_t stride = 1) const requires IsConst
    {
        return FftSliceTemp(_vec, _start + start * _stride, _stride * stride, size);
    }

    FftSliceTemp operator()(std::size_t start, std::size_t size, std::size_t stride = 1) requires (!IsConst)
    {
        return FftSliceTemp(_vec, _start + start * _stride, _stride * stride, size);
    }
};

using CFftSlice = FftSliceTemp<true>;
using FftSlice = FftSliceTemp<false>;

void ditfft2(const CFftSlice& in, FftSlice& out);

#endif /* WFALL_FFT_H */
