#include "dense_scheduler.h"

#include <atomic>

es::DenseScheduler::DenseScheduler(JobIndex capacity) : jobs_capacity(capacity)
{
    assert(capacity % CACHE_LINE_BITS == 0);
    assert(!(capacity & (capacity - 1))); // is power of 2

    matrix = static_cast<word*>(
        ::operator new[](capacity / 8, std::align_val_t{CACHE_LINE_SIZE}));
}

es::DenseScheduler::~DenseScheduler()
{
    ::operator delete[](matrix, std::align_val_t{CACHE_LINE_SIZE});
}

es::DenseScheduler::Worker es::DenseScheduler::create_worker(int thread_id, int num_threads)
{
    assert(thread_id >= 0 && thread_id < num_threads);

    JobIndex step = capacity() / (2 * CACHE_LINE_BITS * num_threads);
    step *= 2 * thread_id; // even

    if (num_threads < 2)
        step = 0;

    return Worker{*this,
        thread_id * (capacity() / num_threads),
        step};
}

