#include "dense_scheduler.h"

#include <atomic>

es::DenseScheduler::DenseScheduler(JobIndex capacity) : jobs_capacity(capacity)
{
    assert(capacity % CACHE_LINE_BITS == 0);
    assert(std::has_single_bit(capacity)); // is power of 2

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

    // linear congruential generator: m * x + 1, m mod 4 == 1
    const unsigned mult = 4 * thread_id + 1;
    assert(mult < capacity() / CACHE_LINE_BITS);

    return Worker{*this,
        thread_id * (capacity() / num_threads),
        mult };
}

