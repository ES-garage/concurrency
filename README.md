# Project description

It is a private dirty fork of original concurrency library to play with Signal Tree benckmarks. The code is not of production quality and is suitable for benchmaring and education only. Fork it and experiment yourself on your own risk. Your computer can burn.

## Usefull hints

- DenseScheduler class to compare benchmarks with SignalTree is in ./src/es
- Fake algorithm is an empty scheduler always returning 0 from pop().
- First comment out benchmarks you do not need in ./src/executable/signal_tree_benchmark/main.cpp
- Benchmark executable is compiled to ./build/bin/signal_tree_benchmark