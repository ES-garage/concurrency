#pragma once

// bench_adapter
//
// Uniform shim that presents a push/pop interface over heterogeneous
// concurrent data structures (MoodyCamel MPMC queue, Intel TBB queue,
// Boost lock-free queue, and signal_tree's signal_set).  This is harness
// scaffolding for the throughput and service-distance benchmarks; it is
// not part of the signal_tree library surface.
//
// Each specialization exposes:
//   - using signal_index   = std::uint64_t;
//   - using selection_hint = std::uint64_t;
//   - explicit bench_adapter(std::size_t capacity);
// Per working thread:
//   - bench_adapter::worker create_worker(std::size_t worker_thread_id, std::size_t num_threads);
//   - void          worker.push(signal_index);
//   - signal_index  worker.pop();
//
// The selection_hint parameter is only used by the signal_tree adapter;
// other backends ignore it.

#include <concurrentqueue.h>
#include <tbb/concurrent_queue.h>
#include <boost/lockfree/queue.hpp>
#include <library/signal_tree.h>
#include <es/dense_scheduler.h>

#include <cstddef>
#include <cstdint>
#include <fmt/format.h>


// signal_tree_size ...
// 0 = max speed
// 1 = balance if using hints for priority
// 2,3,4, etc ... large trees, trades throughput for greater control over prioritization if ever needed
static auto constexpr signal_tree_size = 1;


enum class algorithm
{
    moody_camel,
    signal_tree,
    tbb,
    lockfree,
    dense_scheduler,
    fake
};


template <algorithm>
struct bench_adapter;


//==============================================================================
template <>
struct bench_adapter<algorithm::moody_camel>
{
    using signal_index   = std::uint64_t;

    static std::string name() { return "MoodyCamel"; }

    struct worker {
        moodycamel::ConcurrentQueue<signal_index>& queue;

        void push(signal_index signalId)
        {
            while (!queue.enqueue(signalId))
                ;
        }

        template <typename BeforePublish>
        void push(signal_index signalId, BeforePublish && beforePublish)
        {
            while (true)
            {
                beforePublish();
                if (queue.enqueue(signalId))
                    return;
            }
        }

        signal_index pop()
        {
            signal_index result;
            while (!queue.try_dequeue(result))
                ;
            return result;
        }
    };

    explicit bench_adapter(std::size_t capacity): queue_(capacity * 2){}
    
    worker create_worker(std::size_t worker_thread_id, std::size_t num_threads)
    {
        return worker{queue_};
    }

    moodycamel::ConcurrentQueue<signal_index> queue_;
};


//==============================================================================
template <>
struct bench_adapter<algorithm::tbb>
{
    using signal_index   = std::uint64_t;

    static std::string name() { return "TBB"; }

    struct worker {
        oneapi::tbb::concurrent_queue<signal_index>& queue;

        void push(signal_index signalId)
        {
            queue.push(signalId);
        }

        template <typename BeforePublish>
        void push(signal_index signalId, BeforePublish && beforePublish)
        {
            beforePublish();
            queue.push(signalId);
        }

        signal_index pop()
        {
            signal_index result;
            while (!queue.try_pop(result))
                ;
            return result;
        }
    };

    explicit bench_adapter(std::size_t): queue_(){}

    worker create_worker(std::size_t worker_thread_id, std::size_t num_threads)
    {
        return worker{queue_};
    }

    oneapi::tbb::concurrent_queue<signal_index> queue_;
};


//==============================================================================
template <>
struct bench_adapter<algorithm::lockfree>
{
    using signal_index   = std::uint64_t;

    static std::string name() { return "LockFree"; }
    
    struct worker {
        boost::lockfree::queue<signal_index>& queue;
    
        void push(signal_index signalId)
        {
            while (!queue.push(signalId))
                ;
        }

        template <typename BeforePublish>
        void push(signal_index signalId, BeforePublish && beforePublish)
        {
            while (true)
            {
                beforePublish();
                if (queue.push(signalId))
                    return;
            }
        }

        signal_index pop()
        {
            signal_index result;
            while (!queue.pop(result))
                ;
            return result;
        }
    };

    explicit bench_adapter(std::size_t capacity)
        : queue_(capacity)
    {
    }

    worker create_worker(std::size_t worker_thread_id, std::size_t num_threads)
    {
        return worker{queue_};
    }

    boost::lockfree::queue<signal_index> queue_;
};


//==============================================================================
// signal_tree adapter presents the same uint64 facade as the queue wrappers;
// signal_id conversion is contained here so the benchmark template stays
// algorithm-agnostic.
template <>
struct bench_adapter<algorithm::signal_tree>
{
    using signal_set_type = bcpp::concurrency::signal_set<signal_tree_size>;
    using signal_index    = std::uint64_t;
    using selection_hint = std::uint64_t;

    static std::string name() { return fmt::format("Signal Tree<{}>", signal_set_type::tree_traits::tree_size); }
    
    struct worker {
        signal_set_type& signalSet;
        selection_hint hint;

        void push(signal_index signalId)
        {
            while (not signalSet.set(bcpp::concurrency::signal_id{signalId}))
                ;
        }

        template <typename BeforePublish>
        void push(signal_index signalId, BeforePublish && beforePublish)
        {
            while (true)
            {
                beforePublish();
                if (signalSet.set(bcpp::concurrency::signal_id{signalId}))
                    return;
            }
        }

        signal_index pop()
        {
            auto h  = bcpp::concurrency::signal_id{hint};
            auto id = signalSet.select(h);
            while (not id.valid())
                id = signalSet.select(h);
            hint = static_cast<std::uint64_t>(h);
            return static_cast<std::uint64_t>(id);
        }
    };

    explicit bench_adapter(std::size_t capacity)
        : signalSet_(capacity)
    {
    }

    worker create_worker(std::size_t worker_thread_id, std::size_t num_threads)
    {
        assert(worker_thread_id < num_threads);
        return worker{signalSet_, selection_hint(worker_thread_id * (capacity() / num_threads))};
    }

    std::size_t capacity() const noexcept
    {
        return signalSet_.max() + 1ull;
    }

    signal_set_type signalSet_;
};


//==============================================================================
template <>
struct bench_adapter<algorithm::dense_scheduler>
{
    using signal_index = es::DenseScheduler::JobIndex;

    static std::string name() { return "Dense Scheduler"; }

    struct worker {
        es::DenseScheduler::Worker schedulerWorker;

        void push(signal_index idx)
        {
            schedulerWorker.push(idx);
        }

        template <typename BeforePublish>
        void push(signal_index idx, BeforePublish && beforePublish)
        {
            beforePublish();
            schedulerWorker.push(idx);
        }

        signal_index pop()
        {
            return schedulerWorker.pop();
        }
    };

    explicit bench_adapter(std::size_t capacity) :
        scheduler(capacity)
    {
    }

    worker create_worker(std::size_t worker_thread_id, std::size_t num_threads)
    {
        return worker{scheduler.create_worker(worker_thread_id, num_threads)};
    }

    es::DenseScheduler scheduler;
};

//==============================================================================
// fake adapter presents the same uint64 facade as the queue wrappers;
// Push succeeds immediatly without storing Id. Pop always returns 0.  
template <>
struct bench_adapter<algorithm::fake>
{
    using signal_index = std::uint64_t;

    static std::string name() { return "Fake"; }

    struct worker {
        void push(signal_index)
        {
            inc_counter();
        }

        template <typename BeforePublish>
        void push(signal_index, BeforePublish && beforePublish)
        {
            beforePublish();
            inc_counter();
        }

        signal_index pop()
        {
            inc_counter();
            return 0ull;
        }

        void inc_counter()
        {
            // parent->counter++;
            // counter++;
        }

        bench_adapter<algorithm::fake>* parent;
        std::atomic_int counter{};
        // volatile int counter{};
    };

    explicit bench_adapter(std::size_t capacity)
    {
    }

    worker create_worker(std::size_t worker_thread_id, std::size_t num_threads)
    {
        return worker{this};
    }

    std::atomic_int counter{};
    // int counter{};
};
