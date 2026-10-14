#include "dense_scheduler.h"

#include <atomic>

constexpr int primes[] = {
    1, 5, 13, 17, 29, 37, 41, 53,
    61, 73, 89, 97, 101, 109, 113, 137
};

constexpr int NUM_PRIMES = sizeof(primes) / sizeof(primes[0]);

es::DenseScheduler::DenseScheduler(JobIndex capacity) : jobs_capacity(capacity)
{
    assert(capacity % CACHE_LINE_BITS == 0);

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
    assert(num_threads < NUM_PRIMES);

    return Worker{*this,
        thread_id * (capacity() / num_threads),
        primes[thread_id]};
}

