#include "dense_scheduler.h"

#include <atomic>

static bool has_full_period(uint64_t mult, uint64_t modulus)
{
    uint64_t x = 0;
    uint64_t ctr = 0;

    do {
        x = mult * (x + 1) % modulus;
        if (++ctr > modulus)
            return false;
    } while(x);

    return ctr == modulus;
}

es::DenseScheduler::DenseScheduler(JobIndex capacity) : jobs_capacity(capacity)
{
    assert(capacity % LINE_BITS == 0);
    assert(std::has_single_bit(capacity)); // is power of 2

    matrix = static_cast<word*>(
        ::operator new[](capacity / 8, std::align_val_t{LINE_SIZE}));
}

es::DenseScheduler::~DenseScheduler()
{
    ::operator delete[](matrix, std::align_val_t{LINE_SIZE});
}

es::DenseScheduler::Worker es::DenseScheduler::create_worker(int thread_id, int num_threads)
{
    assert(thread_id >= 0 && thread_id < num_threads);

    // linear congruential generator: m * x + 1, m mod 4 == 1
    const unsigned mult = 4 * thread_id + 1;
    assert(mult < capacity() / LINE_BITS);
    assert(has_full_period(mult, capacity() / LINE_BITS));

    return Worker{*this,
        thread_id * (capacity() / num_threads),
        mult };
}

