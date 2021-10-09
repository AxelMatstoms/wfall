#ifndef WFALL_FFTSEQ_H
#define WFALL_FFTSEQ_H

#include <iostream>
#include <limits>
#include <bit>
#include <complex>
#include <numbers>
#include <vector>
#include <stdexcept>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <optional>

#include "fft.h"

/**
 * Adapts an ifstream to allow non-blocking reads.
 *
 * Starts a thread responsible for interacting with the ifstream. This
 * thread waits for a task, then does that task and waits again,
 * setting flags to indicate state.
 *
 * State diagram:
 * !ready -> waiting -> working -> done -> waiting
 *       (A)        (B)        (C)     (D)
 *
 * (A) worker is started and is now ready.
 * (B) worker receives a task and starts working.
 * (C) worker finishes task and marks done.
 * (D) controller receives result and marks !done.
 */
class NoBlockAdapter {
public:
    enum class Task { none, read, skip, quit };

private:
    std::istream& input;

    char *buffer = nullptr;
    std::size_t buf_cap = 0;

    Task _task = Task::none;
    std::size_t _count = 0;

    std::thread worker_thread;
    mutable std::mutex task_avail_mutex;
    std::condition_variable task_avail;
    bool _ready = false;
    bool _done = false;

    /**
     * Private worker thread.
     */
    void worker();

public:
    /**
     * ctor.
     *
     * Starts worker thread.
     */
    NoBlockAdapter(std::istream& input);

    /**
     * dtor.
     *
     * Tells worker to finish and joins worker thread.
     */
    ~NoBlockAdapter();

    /**
     * Give worker the task of reading count bytes.
     */
    bool read(std::size_t count);

    /**
     * Give worker the task of ignoring count bytes.
     */
    bool skip(std::size_t count);

    /**
     * Mark task as finished.
     */
    void task_finished();

    /**
     * Returns true if task is done but not finished.
     */
    bool done() const;

    /**
     * Returns true if worker is waiting to receive a new task.
     *
     * To ensure worker is actually waiting it is necessary to aquire
     * the task_avail_mutex since the worker unlocks it while waiting.
     */
    bool ready() const;

    /**
     * Return the workers current task.
     */
    Task task() const;

    /**
     * Return the result of a read tsk.
     *
     * Return a ptr pointing to a char buffer containing the bytes read
     * by a read task.
     */
    char* result();
};

/**
 * Print a Task enum to an ostream.
 */
std::ostream& operator<<(std::ostream& out, NoBlockAdapter::Task task);

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
 *
 * Stream has a non-blocking interface where a the first time a
 * member function is called, a command is issued to complete that
 * task. The function can then be called many times to check and
 * possible receive the result of the task. Changing what task is
 * commanded, or the parameters to that task is undefined behaviour.
 * Once a command has been made, that same command must be repeated
 * until confirmation that it has been completed is received.
 */
struct Stream {
    using OutSample = std::complex<float>;

    /**
     * Read count frames.
     *
     * For multichannel data each frame (consisting of one sample for
     * each channel) must be processed in some way to yield a single
     * complex number.
     *
     * Returns an empty vector until task is done.
     */
    virtual std::vector<OutSample> read_chunk(std::size_t count) = 0;

    /**
     * Skip count frames.
     *
     * This can be implemented so that it is cheaper to use this method
     * than to simply ignore output from Stream::read.
     *
     * Returns false until task is done.
     */
    virtual bool skip(std::size_t count) = 0;
};

/**
 * Parses PCM data.
 */
template <typename Sample>
class PcmStream : Stream {
    using Stream::OutSample;

    NoBlockAdapter _input;
    std::size_t _channels = 1;
    std::endian _endian = std::endian::little;

    std::size_t _solo;
    bool _mix;
    bool _iq;

public:
    PcmStream(std::istream& input) : _input(input) {}

    std::size_t channels() const { return _channels; }
    void channels(std::size_t count) { _channels = count; }

    std::endian endian() const { return _endian; }
    void endian(std::endian order) { _endian = order; }

    bool is_solo() const { return _mix || _iq; }
    std::size_t get_solo() const { return _solo; }
    void solo(std::size_t ch)
    {
        if (ch >= _channels)
            throw std::out_of_range("Solo channel index out of bounds");

        _solo = ch;
        _mix = false;
        _iq = false;
    }

    bool is_mix() const { return _mix; }
    void mix() {
        _mix = true;
        _iq = false;
    }

    bool is_iq() const { return _iq; }
    void iq()
    {
        if (_channels < 2) {
            throw std::logic_error("IQ data needs at least 2 channels");
        }
        _iq = true;
        _mix = false;
    }

private:
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

    float pcm_convert(Sample sample)
    {
        if constexpr (std::is_signed_v<Sample>) {
            return (float) sample / -((float) std::numeric_limits<Sample>::min());
        } else {
            return (float) sample / (float) (std::numeric_limits<Sample>::max() / 2) - 1.0f;
        }
    }

    OutSample parse_mix(char *ptr)
    {
        OutSample out_sample = 0.0f;
        for (std::size_t c = 0; c < _channels; c++) {
            Sample in_sample = *reinterpret_cast<Sample*>(ptr + c * sizeof(Sample));
            out_sample += pcm_convert(in_sample);
        }

        return out_sample / float(_channels);
    }

    OutSample parse_iq(char *ptr)
    {
        OutSample out_sample = 0.0f;

        Sample in_sample = *reinterpret_cast<Sample*>(ptr);
        out_sample.real(pcm_convert(in_sample));
        in_sample = *reinterpret_cast<Sample*>(ptr + sizeof(Sample));
        out_sample.imag(pcm_convert(in_sample));

        return out_sample;
    }

    OutSample parse_solo(char *ptr)
    {
        Sample in_sample = *reinterpret_cast<Sample*>(ptr + _solo * sizeof(Sample));
        return pcm_convert(in_sample);
    }

public:
    std::vector<OutSample> read_chunk(std::size_t count) override
    {
        std::size_t total_size = count * _channels * sizeof(Sample);

        char *buffer;
        if (!_input.done()) {
            if (_input.ready()) {
                _input.read(total_size);
            }

            return std::vector<OutSample>();
        }

        buffer = _input.result();

        if (_endian != std::endian::native) {
            bswap_buffer(buffer, total_size);
        }

        std::vector<OutSample> out;
        out.reserve(count);
        for (std::size_t i = 0; i < count; i++) {
            char *ptr = buffer + i * _channels * sizeof(Sample);

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

    bool skip(std::size_t count) override
    {
        std::size_t total_size = count * _channels * sizeof(Sample);

        if (!_input.done()) {
            if (_input.ready()) {
                _input.skip(total_size);
            }

            return false;
        }

        return true;
    }
};

class FftSeq {
    Stream& stream;

    std::size_t _fft_size;
    int _gap;

public:
    FftSeq(Stream& stream);

    void consecutive();
    void fftrate(float sample_rate, float rate);
};

#endif /* WFALL_FFTSEQ_H */
