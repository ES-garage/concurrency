#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <cassert>
#include <atomic>
#include <bit>

#include <xmmintrin.h>

namespace es {
    class DenseScheduler {
        using word = uint64_t;
    public:
        using JobIndex = std::uint64_t;
        static constexpr JobIndex INVALID_JOB_INDEX = std::numeric_limits<JobIndex>::max();

        explicit DenseScheduler(JobIndex capacity);
        ~DenseScheduler();

        DenseScheduler(const DenseScheduler&) = delete;
        DenseScheduler& operator=(const DenseScheduler&) = delete;

        class Worker {
        public:
            void push(JobIndex id);
            
            static constexpr JobIndex NO_JOB_INDEX = std::numeric_limits<JobIndex>::max();
            JobIndex pop();

            JobIndex nextCandidate() const { return next_candidate; }
            void setNextCandidate(JobIndex next) { assert(next < parent.capacity()); next_candidate = next; }

            DenseScheduler& scheduler() { return parent; }
            const DenseScheduler& scheduler() const { return parent; }

        private:
            word* matrix;
            JobIndex next_candidate{0};
            JobIndex total_words;
            unsigned  step_mult;
            DenseScheduler& parent;

            Worker(DenseScheduler& scheduler, JobIndex start, unsigned step) :
                matrix(scheduler.matrix),
                total_words(scheduler.capacity() / WORD_BITS),
                step_mult(step),
                parent(scheduler)
            {
                setNextCandidate((step * CACHE_LINE_BITS) % parent.capacity());
            }

            friend class DenseScheduler;
        };
        
        Worker create_worker(int thread_id, int num_threads);
        JobIndex capacity() const { return jobs_capacity; }
    
    private:
        static constexpr unsigned CACHE_LINE_SIZE = 64;
        static constexpr unsigned CACHE_LINE_BITS = CACHE_LINE_SIZE * 8;
        static constexpr unsigned CACHE_LINE_WORDS = CACHE_LINE_SIZE / sizeof(word); 
        static constexpr unsigned WORD_BITS = sizeof(word) * 8;

        JobIndex jobs_capacity;
        word* matrix = nullptr;
    };
}

inline void es::DenseScheduler::Worker::push(JobIndex id)
{
    assert(id < parent.capacity());

    std::atomic_ref<word> w{matrix[id / WORD_BITS]};
    w.fetch_or(word{1} << (id % WORD_BITS), std::memory_order_release);
}

inline es::DenseScheduler::JobIndex es::DenseScheduler::Worker::pop()
{
    assert(next_candidate < parent.capacity());
    
    JobIndex word_idx = next_candidate / WORD_BITS;
    unsigned bit_idx = next_candidate % WORD_BITS;
    
    JobIndex visited_cache_lines = 0;
    JobIndex word_idx_mask = total_words - 1;
    word mask = word{1} << bit_idx;

    while (true) {
        std::atomic_ref<word> aw{matrix[word_idx]};
        word w = aw.fetch_and(~mask, std::memory_order_acquire);
        
        // Find the next 1 to the left. 
        word next_w = w & ~mask & -mask; // ignore old low bits
        word next_mask = next_w & -next_w; // rightmost one

        if (w & mask) {
            // active job is found - return a word with single bit set
            bit_idx = std::countr_zero(mask);
            assert(bit_idx < WORD_BITS);

            JobIndex found_job = word_idx * WORD_BITS + bit_idx;
        
            mask = next_mask;
            if (mask) {
                bit_idx = std::countr_zero(mask);
            } else {
                bit_idx = 0;

                if (!(++word_idx & (CACHE_LINE_WORDS - 1))) {
                    // next word is the first word in a cache line
                    word_idx *= step_mult;
                    word_idx &= word_idx_mask;

                    _mm_prefetch(matrix + (((word_idx + CACHE_LINE_WORDS) * step_mult) & word_idx_mask), _MM_HINT_T0);
                }
            }
        
            next_candidate = word_idx * WORD_BITS + bit_idx;

            assert(found_job < parent.capacity());
            assert(next_candidate < parent.capacity());

            return found_job;
        }    

        mask = next_mask;
        if (!mask) {
            // to the next word
            mask = 1;
            if (!(++word_idx & (CACHE_LINE_WORDS - 1))) {
                if (++visited_cache_lines > total_words / CACHE_LINE_WORDS)
                    return NO_JOB_INDEX;

                word_idx *= step_mult;
                word_idx &= word_idx_mask;
                
                _mm_prefetch(matrix + (((word_idx + CACHE_LINE_WORDS) * step_mult) & word_idx_mask), _MM_HINT_T0);
            }
       }
    }
}

