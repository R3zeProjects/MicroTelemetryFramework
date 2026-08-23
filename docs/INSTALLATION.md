# Установка

MTF — это пакет header-only C++23, требующий CMake 3.25, потоков и
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

Установите MCF и MTF в один префикс, затем используйте их с помощью:

```cmake
find_package(mtf 0.1 REQUIRED CONFIG)
target_link_libraries(application PRIVATE vosp::telemetry)
```

Пропустите префикс через `CMAKE_PREFIX_PATH`. Основной заголовок — `<vosp/telemetry.hpp>`. Установите `MTF_BUILD_EXAMPLES=OFF` и `BUILD_TESTING=OFF` при
встраивании MTF в качестве зависимости. Бенчмарки и закрепленные сравнения никогда не
устанавливаются.
