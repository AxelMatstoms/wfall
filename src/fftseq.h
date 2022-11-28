#ifndef WFALL_FFTSEQ_H
#define WFALL_FFTSEQ_H

#include <iostream>
#include <limits>
#include <bit>
#include <complex>
#include <numbers>
#include <vector>
#include <stdexcept>
#include <functional>
#include <thread>
#include <atomic>

#include "fft.h"

/**
 * Byteorder swap for 2-byte ints.
 */
void bswap2(char* ptr);

/**
 * Byteorder swap for 4-byte ints.
 */
void bswap4(char* ptr);

/**
 * An abstract base class for an input stream to an FftSeq.
 *
 * This is needed since PcmStream is templated on the sample format
 * and FftSeq does not depend on the sample format.
 */
struct Stream {
    using OutSample = std::complex<float>;

    /**
     * Read count frames.
     *
     * For multichannel data each frame (consisting of one sample for
     * each channel) must be processed in some way to yield a single
     * complex number.
     */
    virtual std::vector<OutSample> read_chunk(std::size_t count) = 0;

    /**
     * Skip count frames.
     *
     * This can be implemented so that it is cheaper to use this method
     * than to simply ignore output from Stream::read.
     */
    virtual void skip(std::size_t count) = 0;
};

/**
 * Parses PCM data.
 *
 * The sample format is specified with a template parameter.
 * PcmStream has three ways of handling multichannel data:
 * solo: the output stream is one of the channels from the input.
 * mix: the output stream is the average of the input channels.
 * iq: the real part of the output is the first channel and the
 * imaginary part of the output is the second channel.
 *
 * The term frame is used to mean a sequence of n samples where n is
 * the number of channels in the stream. The frame is aligned so that
 * the first sample in the frame comes from the first channel.
 */
template <typename Sample>
class PcmStream : public Stream {
    using Stream::OutSample;

    std::istream& _input;
    std::size_t _channels = 1;
    std::endian _endian = std::endian::little;

    std::size_t _solo;
    bool _mix;
    bool _iq;

    std::vector<char> buf;

public:
    /**
     * ctor.
     *
     * Constructs a PcmStream from an istream.
     */
    PcmStream(std::istream& input) : _input(input) {}

    /**
     * Getter for the number of channels.
     */
    std::size_t channels() const { return _channels; }

    /**
     * Setter for the number of channels.
     */
    void channels(std::size_t count) { _channels = count; }

    /**
     * Getter for the endianness.
     */
    std::endian endian() const { return _endian; }

    /**
     * Setter for the endianness.
     */
    void endian(std::endian order) { _endian = order; }

    /**
     * Returns true if the PcmStream is in solo mode.
     */
    bool is_solo() const { return _mix || _iq; }

    /**
     * Getter for the selected solo channel.
     *
     * Only valid if is_solo() returns true.
     */
    std::size_t solo() const { return _solo; }

    /**
     * Sets the PcmStream to solo mode and selects the given channel.
     */
    void solo(std::size_t ch)
    {
        if (ch >= _channels)
            throw std::out_of_range("Solo channel index out of bounds");

        _solo = ch;
        _mix = false;
        _iq = false;
    }

    /**
     * Returns true if the PcmStream is in mix mode
     */
    bool is_mix() const { return _mix; }

    /**
     * Sets the PcmStream to mix mode.
     */
    void mix() {
        _mix = true;
        _iq = false;
    }

    /**
     * Returns true if the PcmStream is in iq mode
     */
    bool is_iq() const { return _iq; }

    /**
     * Sets the PcmStream to iq mode.
     *
     * Throws an error if there is only one input channel.
     */
    void iq()
    {
        if (_channels < 2) {
            throw std::logic_error("IQ data needs at least 2 channels");
        }
        _iq = true;
        _mix = false;
    }

private:
    /**
     * Swaps the bytes in buffer, depending on the sample width.
     */
    void bswap_buffer(char *buffer, std::size_t size)
    {
        if (sizeof(Sample) == 2) {
            for (std::size_t i = 0; i < size; i += 2) {
                bswap2(buffer + i);
            }
        } else if (sizeof(Sample) == 4) {
            for (std::size_t i = 0; i < size; i += 4) {
                bswap4(buffer + i);
            }
        }
    }

    /**
     * Converts a PCM sample to a normalized float (-1, 1)
     *
     * Signed sample formats are converted by dividing by the negative
     * of the minimum value of that format. For example: signed 8-bit
     * PCM is converted by dividing by 128.
     *
     * Unsigned sample formats are converted by dividing by half of the
     * maximum value and subtracting 1.0f. Samples encoding amplitude
     * zero are handled correctly.
     */
    float pcm_convert(Sample sample)
    {
        if constexpr (std::is_floating_point_v<Sample>) {
            return sample;
        } else if constexpr (std::is_signed_v<Sample>) {
            return (float) sample / -((float) std::numeric_limits<Sample>::min());
        } else {
            return (float) sample / (float) (std::numeric_limits<Sample>::max() / 2) - 1.0f;
        }
    }

    /**
     * Parses a pcm frame, returning the average of all channels.
     */
    OutSample parse_mix(char *ptr)
    {
        OutSample out_sample = 0.0f;
        for (std::size_t c = 0; c < _channels; c++) {
            Sample in_sample = *reinterpret_cast<Sample*>(ptr + c * sizeof(Sample));
            out_sample += pcm_convert(in_sample);
        }

        return out_sample / float(_channels);
    }

    /**
     * Parses a pcm frame, returning the frame interpreted as an iq
     * sample.
     */
    OutSample parse_iq(char *ptr)
    {
        OutSample out_sample = 0.0f;

        Sample in_sample = *reinterpret_cast<Sample*>(ptr);
        out_sample.real(pcm_convert(in_sample));
        in_sample = *reinterpret_cast<Sample*>(ptr + sizeof(Sample));
        out_sample.imag(pcm_convert(in_sample));

        return out_sample;
    }

    /**
     * Parses one of samples in a frame and returns its amplitude.
     */
    OutSample parse_solo(char *ptr)
    {
        Sample in_sample = *reinterpret_cast<Sample*>(ptr + _solo * sizeof(Sample));
        return pcm_convert(in_sample);
    }

public:
    /**
     * Read a chunk of pcm data and convert it to floating point.
     *
     * Reads count frames from the stream, decoding each and
     * returns a vector containing complex numbers that are
     * the input for an fft.
     */
    std::vector<OutSample> read_chunk(std::size_t count) override
    {
        std::size_t total_size = count * _channels * sizeof(Sample);

        if (total_size != buf.size()) {
            buf.resize(total_size);
        }

        _input.read(buf.data(), total_size);

        if (_endian != std::endian::native) {
            bswap_buffer(buf.data(), total_size);
        }

        std::vector<OutSample> out;
        out.reserve(count);
        for (std::size_t i = 0; i < count; i++) {
            char *ptr = buf.data() + i * _channels * sizeof(Sample);

            if (_mix) {
                out.push_back(parse_mix(ptr));
            } else if (_iq) {
                out.push_back(parse_iq(ptr));
            } else {
                out.push_back(parse_solo(ptr));
            }
        }

        return out;
    }

    /**
     * Skip count frames of input data.
     */
    void skip(std::size_t count) override
    {
        std::size_t total_size = count * _channels * sizeof(Sample);
        _input.ignore(total_size);
    }
};

std::vector<float> blackman(std::size_t N);
std::vector<float> rectangular(std::size_t N);

/**
 * Asynchronously computes consecutive FFTs of a signal.
 *
 * Uses a thread for FFT computation and I/O. Applies the chosen window
 * function to the input data before running the FFT.
 * The basic usage is as following:
 *
 * FftSeq fft_seq(...);
 * fft_seq.start();
 * while (...) {
 *   if (fft_seq.has_next()) {
 *     // Move the fft to v.
 *     auto v = fft_seq.next();
 *     // Notify thread to start working again.
 *     fft_seq.notify();
 *   }
 * }
 *
 * After finishing its computation the thread waits until notify is
 * called.
 */
class FftSeq {
public:
    using WinFn = std::function<std::vector<float>(std::size_t)>;

private:
    Stream& _stream;
    std::size_t _fft_size;
    int _spacing = 0;
    WinFn _window_fn;
    std::vector<std::complex<float>> _result;
    std::thread _worker;
    std::atomic<bool> _done;
    bool _quit = false;

    void worker_fn();

public:
    FftSeq(Stream& stream, std::size_t fft_size, const WinFn& win_fn = blackman);
    ~FftSeq();

    void start();

    std::size_t fft_size() const;

    void spacing(int spacing);
    int spacing() const;

    void optimal_spacing(float srate, float fft_rate);

    bool has_next() const;
    std::vector<std::complex<float>>&& next();
    void notify();
};

#endif /* WFALL_FFTSEQ_H */
