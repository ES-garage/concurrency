#pragma once

#if !defined(__linux__) && defined(BCPP_REQUIRE_CPU_AFFINITY)
#error "BCPP_REQUIRE_CPU_AFFINITY requires Linux CPU affinity support"
#endif

#if defined(__linux__)
#include <library/system.h>
#endif

#include <cstddef>
#include <iostream>


namespace benchmark_affinity
{

    inline constexpr bool enabled =
#if defined(__linux__)
        true;
#else
        false;
#endif

    inline constexpr auto ansi_bold  = "\033[1m";
    inline constexpr auto ansi_reset = "\033[0m";


    inline char const * policy() noexcept
    {
#if defined(__linux__)
        return "linux";
#else
        return "disabled";
#endif
    }


    inline void set_cpu_affinity(int threadId) noexcept
    {
#if defined(__linux__)
        bcpp::system::set_cpu_affinity(threadId);
#else
        (void)cpuId;
#endif
    }

    inline void print_environment_once(std::size_t numThreads, std::ostream & os = std::cout)
    {
        static auto printed = false;

        if (printed)
            return;

        printed = true;

        os << "==================================================================================\n";
        os << ansi_bold;
        os << "Benchmark Environment\n";
        os << ansi_reset;
        os << "Number of threads: " << numThreads;
        os << "\n\n";
    }

} // namespace benchmark_affinity
