# Installation

MTF is a header-only C++23 package requiring CMake 3.25, threads and
MicroContractsFramework.

```sh
git clone https://github.com/R3zeProjects/MicroContractsFramework.git
git clone https://github.com/R3zeProjects/MicroTelemetryFramework.git
cmake -S MicroTelemetryFramework -B build/mtf -DCMAKE_BUILD_TYPE=Release \
  -DMTF_CONTRACTS_SOURCE_DIR=/absolute/path/to/MicroContractsFramework \
  -DBUILD_TESTING=ON -DMTF_BUILD_EXAMPLES=ON
cmake --build build/mtf --parallel
ctest --test-dir build/mtf --output-on-failure
```

Install MCF and MTF into the same prefix, then consume them with:

```cmake
find_package(mtf 0.1 REQUIRED CONFIG)
target_link_libraries(application PRIVATE vosp::telemetry)
```

Pass the prefix through `CMAKE_PREFIX_PATH`. The umbrella header is
`<vosp/telemetry.hpp>`. Set `MTF_BUILD_EXAMPLES=OFF` and `BUILD_TESTING=OFF`
when embedding MTF as a dependency. Benchmarks and pinned comparisons are never
installed.
