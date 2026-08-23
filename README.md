# MicroTelemetryFramework

MicroTelemetryFramework (MTF) — это небольшой C++23 фреймворк для сбора и экспорта
метрик, событий и завершённых spans без привязки приложения к конкретному backend
наблюдаемости.

> Постройте один контур телеметрии вокруг любой подсистемы, затем замените его exporter
> без переписывания инструментов.

Текущая версия выпуска — **0.1.1-beta**. MTF — это header-only и поддерживает GCC, Clang
и MSVC через CMake 3.25 или новее.

## Возможности

- Атомарные thread-safe дескрипторы `Counter` и `Gauge`;
- `Histogram` с фиксированными границами и согласованными snapshots;
- Ограниченный, thread-safe `Registry` с уникальными названиями инструментов;
- Владеющие значения `Record` для метрик, событий и завершённых spans;
- Синхронный экспорт без очереди фреймворка;
- Ограниченный асинхронный экспорт с блокировкой backpressure и пакетной обработкой;
- Явные `flush()`, draining `shutdown()` и счётчики доставки;
- Runtime-polymorphic `IExporter` и thread-safe `MemoryExporter`;
- Проверка compile-time протоколов через MicroContractsFramework;
- Отсутствие зависимости от транспорта, базы данных, logging backend или
  глобального синглтона.

## Быстрый старт

```cpp
#include <vosp/telemetry.hpp>

#include <memory>

int main()
{
    auto exporter = std::make_shared<vosp::telemetry::MemoryExporter>();
    vsp::Telemetry telemetry{exporter};
    vosp::telemetry::Registry metrics;

    auto requests = metrics.counter("http.requests", {{"service", "gateway"}});
    auto latency = metrics.histogram("http.latency_ms", {1.0, 5.0, 25.0, 100.0});

    static_cast<void>(requests.add());
    static_cast<void>(latency.observe(4.2));
    static_cast<void>(telemetry.collect(metrics));
    static_cast<void>(telemetry.publish(
        vosp::telemetry::Record::event("service.ready")));
}
```

Для ограниченного фонового экспорта измените только политику:

```cpp
using Async = vosp::telemetry::pipeline_policy::Async<1024, 64>;
vsp::TelemetryPipeline<Async> telemetry{exporter};
```

Первый параметр — это емкость очереди, а второй — максимальный размер пакета экспорта.
Производители блокируются, когда очередь полна; принятые записи обрабатываются до
завершения shutdown.

## Зависимость

Полная процедура автономной и установленной версии задокументирована в [installation
guide](docs/INSTALLATION.md). Прямые, асинхронные и пользовательские примеры exporter
собраны в [usage examples](docs/USAGE_EXAMPLES.md).

MTF зависит от
[MicroContractsFramework](https://github.com/R3zeProjects/MicroContractsFramework)
`0.5.x` для концепций структурной телеметрии. MCF не владеет состоянием телеметрии
runtime. MEF и MPF не требуются ядром MTF.

```cmake
find_package(mtf 0.1 REQUIRED CONFIG)
target_link_libraries(your_target PRIVATE vosp::telemetry)
```

Для разработки дерева исходного кода:

```sh
cmake -S . -B build \
  -DMTF_CONTRACTS_SOURCE_DIR=/path/to/MicroContractsFramework \
  -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Без `MTF_CONTRACTS_SOURCE_DIR` CMake сначала ищет установленный пакет
`vosp_contracts 0.5`, а иначе получает закреплённый совместимый коммит.

## Измеренная базовая линия

Результаты локального выпуска на AMD Ryzen 7 PRO 1700X (8 ядер / 16 потоков), Clang
22.1.6, Windows. Медиана пяти запусков, 1,000,000 операций за запуск:

| Сценарий | Медианная пропускная способность |
| --- | ---: |
| Обновление атомарного счётчика | **119.593M операций/с** |
| Экспорт ограниченных асинхронных событий, q1024/b64 | **1.985M записей/с** |

exporter учитывает только доставленные записи, поэтому асинхронный результат измеряет
MTF построение записей, постановку в очередь, синхронизацию, пакетирование и доставку.
Эти цифры являются воспроизводимой базой, а не гарантией между машинами. Бенчмарки
предназначены для инструментов репозитория и не устанавливаются вместе с пакетом.

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release \
  -DMTF_BUILD_BENCHMARKS=ON
cmake --build build-release --parallel
./build-release/MicroTelemetryFrameworkBenchmark
```

## Внешнее сравнение

Сравнение закрепленных релизов на AMD Ryzen 7 PRO 1700X с Clang 22.1.6 на Windows,
медиана семи прогонов с 1,000,000 обновлениями за прогон:

| Библиотека | Обновления счетчика/с | Гистограмма наблюдений/с |
| --- | ---: | ---: |
| Базовый уровень `std::atomic<double>` | **123.805M** | — |
| MTF 0.1.1 | **123.790M** | **55.745M** |
| prometheus-cpp 1.3.0 | **129.835M** | **26.378M** |
| OpenTelemetry C++ SDK 1.9.1 | **34.583M** | **27.366M** |

Счетчик MTF находится в пределах шумов измерений относительно атомной базовой линии, а
4.7% ниже prometheus-cpp в этом запуске. Его гистограмма с фиксированными границами
измерила 2.11 раза скорость prometheus-cpp и 2.04 раза скорость OpenTelemetry SDK.

Это сравнение «горячих» путей, а не утверждение о том, что MTF заменяет OpenTelemetry.
OpenTelemetry также предоставляет стандартизированные семантические соглашения,
распространение контекста, процессоры и сетевые exporters, которые MTF 0.1.1 не
реализует. Все реализации агрегируют один немаркированный счетчик или гистограмму;
проверка сбора данных происходит вне измеряемого региона. MTF и prometheus-cpp
используют одни и те же четыре явных границы корзин, в то время как OpenTelemetry
использует агрегацию гистограмм по умолчанию в SDK, так что строка гистограммы
сравнивает практические «горячие» пути, а не идентичные внутренние структуры данных.

## Полный контур экосистемы

Закреплённая нагрузка MCF + MEF + MPF + MTF выполняет, на каждую транзакцию:

1. Один структурированный журнал ошибок MEF;
2. Одно обновление счетчика MTF и одно событие владения;
3. Persistence журнала и события как два MPF записи;
4. Проверка каждой принятой и сохраненной записи.

| Режим | Медианные транзакции/с | Сохранённые записи/с |
| --- | ---: | ---: |
| Прямой экспорт телеметрии | **5.685M** | **11.369M** |
| Ограниченный асинхронный q1024/b64 | **1.609M** | **3.218M** |

Прямой режим быстрее, потому что журнал в памяти дешев; асинхронный режим оплачивает
ожидание в очереди и затраты владение, не скрывая медленный ввод-вывод. Асинхронный
режим остаётся ценным, когда реальный exporter блокируется или имеет пиковую
латентность.

## Документация

- [Architecture](docs/ARCHITECTURE.md)
- [Installation](docs/INSTALLATION.md)
- [Usage examples](docs/USAGE_EXAMPLES.md)
- [API and concurrency contracts](docs/API_CONTRACTS.md)
- [Benchmark methodology](docs/BENCHMARKS.md)
- [Ecosystem capability and maturity assessment](docs/ECOSYSTEM.md)

MTF лицензирован на условиях лицензии MIT.
