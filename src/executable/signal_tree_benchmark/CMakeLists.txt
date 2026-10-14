
add_executable(
    signal_tree_benchmark
    main.cpp
)

target_include_directories(signal_tree_benchmark
PRIVATE
    ${concurrentqueue_SOURCE_DIR}
    ${lockfree_SOURCE_DIR}/include
)

target_link_libraries(signal_tree_benchmark
PRIVATE
    bcpp::signal_tree
    fmt
    tbb
    system
    es
)

target_compile_options(signal_tree_benchmark PRIVATE
    $<$<CONFIG:Debug>:-fsanitize=address>
    $<$<CONFIG:Debug>:-fno-omit-frame-pointer>
    -march=native
)

target_link_options(signal_tree_benchmark PRIVATE
    $<$<CONFIG:Debug>:-fsanitize=address>
)
