#include "fftseq.h"

NoBlockAdapter::NoBlockAdapter(std::istream& input) : input(input)
{
    worker_thread = std::thread(&NoBlockAdapter::worker, this);
}

NoBlockAdapter::~NoBlockAdapter()
{
    if (buffer != nullptr)
        delete[] buffer;

    // quit and join
    task_avail_mutex.lock();
    task_avail_mutex.unlock();
    _task = Task::quit;
    task_avail.notify_one();
    worker_thread.join();
}

bool NoBlockAdapter::read(std::size_t count)
{
    if (!ready()) {
        return false;
    }

    _task = Task::read;
    _count = count;
    _done = false;

    task_avail_mutex.unlock();
    task_avail.notify_one();

    return true;
}

bool NoBlockAdapter::skip(std::size_t count)
{
    if (!ready()) {
        return false;
    }

    _task = Task::skip;
    _count = count;
    _done = false;

    task_avail_mutex.unlock();
    task_avail.notify_one();

    return true;
}

void NoBlockAdapter::task_finished()
{
    _done = false;
}

bool NoBlockAdapter::done() const
{
    return _done;
}

bool NoBlockAdapter::ready()
{
    if (_ready && task_avail_mutex.try_lock()) {
        task_avail_mutex.unlock();
        return true;
    }

    return false;
}

NoBlockAdapter::Task NoBlockAdapter::task() const
{
    return _task;
}

char* NoBlockAdapter::result()
{
    return buffer;
}

void NoBlockAdapter::worker()
{
    std::unique_lock<std::mutex> lk(task_avail_mutex);
    while (true) {
        _ready = true;
        task_avail.wait(lk, [this]{ return _task != Task::none; });
        _ready = false;

        std::cout << "Got work _task=" << _task << " _count=" << _count << std::endl;

        if (_task == Task::quit) {
            break;
        } else if (_task == Task::read) {
            if (buf_cap < _count) {
                if (buffer != nullptr)
                    delete[] buffer;
                buffer = new char[_count];
                buf_cap = _count;
                std::cout << "Allocting new buffer of size " << _count << std::endl;
            }

            input.read(buffer, _count);
            _done = true;
        } else if (_task == Task::skip) {
            input.ignore(_count);
            _done = true;
        }

        _task = Task::none;
    }
}

std::ostream& operator<<(std::ostream& out, NoBlockAdapter::Task task)
{
    using Task = NoBlockAdapter::Task;

    switch (task) {
        case Task::none:
            out << "Task::none";
            break;
        case Task::read:
            out << "Task::read";
            break;
        case Task::skip:
            out << "Task::skip";
            break;
        case Task::quit:
            out << "Task::quit";
            break;
    }

    return out;
}

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
