cmake_minimum_required(VERSION 3.25)

project(MicroTelemetryFramework VERSION 0.1.1 LANGUAGES CXX)

set(MICRO_TELEMETRY_FRAMEWORK_PRERELEASE "beta")

include(CTest)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
find_package(Threads REQUIRED)

option(MTF_BUILD_EXAMPLES "Build MTF examples" ON)
option(MTF_BUILD_BENCHMARKS "Build opt-in MTF benchmarks" OFF)
option(MTF_BUILD_EXTERNAL_COMPARISON_BENCHMARKS
    "Build pinned prometheus-cpp and OpenTelemetry comparisons" OFF)
option(MTF_BUILD_ECOSYSTEM_BENCHMARKS
    "Build the pinned MCF+MEF+MPF+MTF end-to-end benchmark" OFF)
option(MTF_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(MTF_ENABLE_THREAD_SANITIZER "Enable ThreadSanitizer" OFF)
option(MTF_FETCH_CONTRACTS "Fetch pinned MCF contracts when not installed" ON)
set(MTF_CONTRACTS_SOURCE_DIR "" CACHE PATH "Optional MicroContractsFramework source tree")

if(MTF_ENABLE_SANITIZERS AND MTF_ENABLE_THREAD_SANITIZER)
    message(FATAL_ERROR "ASan/UBSan and ThreadSanitizer require separate builds")
endif()

if(NOT TARGET vosp::contracts)
    if(MTF_CONTRACTS_SOURCE_DIR)
        if(NOT EXISTS "${MTF_CONTRACTS_SOURCE_DIR}/CMakeLists.txt")
            message(FATAL_ERROR "MTF_CONTRACTS_SOURCE_DIR does not contain MCF")
        endif()
        set(_mtf_saved_build_testing "${BUILD_TESTING}")
        set(BUILD_TESTING OFF)
        add_subdirectory("${MTF_CONTRACTS_SOURCE_DIR}"
                         "${CMAKE_CURRENT_BINARY_DIR}/contracts" EXCLUDE_FROM_ALL)
        set(BUILD_TESTING "${_mtf_saved_build_testing}")
        unset(_mtf_saved_build_testing)
    else()
        find_package(vosp_contracts 0.5 QUIET CONFIG)
        if(NOT vosp_contracts_FOUND)
            if(NOT MTF_FETCH_CONTRACTS)
                message(FATAL_ERROR
                    "MicroContractsFramework 0.5 is required. Install it, set "
                    "MTF_CONTRACTS_SOURCE_DIR, or enable MTF_FETCH_CONTRACTS.")
            endif()
            include(FetchContent)
            FetchContent_Declare(vosp_contracts
                GIT_REPOSITORY https://github.com/R3zeProjects/MicroContractsFramework.git
                GIT_TAG e69892633f0d863424a5b76c9730c2456691e564
                GIT_SHALLOW FALSE)
            set(_mtf_saved_build_testing "${BUILD_TESTING}")
            set(BUILD_TESTING OFF)
            FetchContent_MakeAvailable(vosp_contracts)
            set(BUILD_TESTING "${_mtf_saved_build_testing}")
            unset(_mtf_saved_build_testing)
        endif()
    endif()
endif()

add_library(MicroTelemetryFramework INTERFACE)
add_library(vosp::telemetry ALIAS MicroTelemetryFramework)
set_target_properties(MicroTelemetryFramework PROPERTIES EXPORT_NAME telemetry)
target_compile_features(MicroTelemetryFramework INTERFACE cxx_std_23)
target_include_directories(MicroTelemetryFramework INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(MicroTelemetryFramework INTERFACE
    vosp::contracts Threads::Threads)

target_compile_options(MicroTelemetryFramework INTERFACE
    $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/W4;/permissive->
    $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:-Wall;-Wextra;-Wpedantic>
)

function(mtf_enable_sanitizers target)
    if(MTF_ENABLE_SANITIZERS)
        if(MSVC)
            target_compile_options(${target} PRIVATE /fsanitize=address)
            target_link_options(${target} PRIVATE /fsanitize=address)
        else()
            target_compile_options(${target} PRIVATE
                -fsanitize=address,undefined -fno-omit-frame-pointer)
            target_link_options(${target} PRIVATE -fsanitize=address,undefined)
        endif()
    elseif(MTF_ENABLE_THREAD_SANITIZER)
        if(MSVC)
            message(FATAL_ERROR "ThreadSanitizer is unavailable with MSVC")
        endif()
        target_compile_options(${target} PRIVATE -fsanitize=thread -fno-omit-frame-pointer)
        target_link_options(${target} PRIVATE -fsanitize=thread)
    endif()
endfunction()

if(BUILD_TESTING)
    add_library(MicroTelemetryFrameworkHeaderContracts OBJECT
        tests/header_contracts/record_header.cpp
        tests/header_contracts/pipeline_header.cpp)
    target_link_libraries(MicroTelemetryFrameworkHeaderContracts
        PRIVATE MicroTelemetryFramework)

    add_executable(MicroTelemetryFrameworkTests tests/telemetry_tests.cpp)
    target_link_libraries(MicroTelemetryFrameworkTests PRIVATE MicroTelemetryFramework)
    mtf_enable_sanitizers(MicroTelemetryFrameworkTests)
    add_test(NAME MicroTelemetryFramework.Tests COMMAND MicroTelemetryFrameworkTests)
endif()

if(MTF_BUILD_EXAMPLES)
    add_executable(MicroTelemetryFrameworkExample examples/basic.cpp)
    target_link_libraries(MicroTelemetryFrameworkExample PRIVATE MicroTelemetryFramework)
    add_executable(MicroTelemetryFrameworkCompleteExample examples/complete.cpp)
    target_link_libraries(MicroTelemetryFrameworkCompleteExample PRIVATE MicroTelemetryFramework)
    if(BUILD_TESTING)
        add_test(NAME MicroTelemetryFramework.Example.Complete
            COMMAND MicroTelemetryFrameworkCompleteExample)
    endif()
endif()

if(MTF_BUILD_BENCHMARKS)
    add_executable(MicroTelemetryFrameworkBenchmark benchmarks/mtf_benchmark.cpp)
    target_link_libraries(MicroTelemetryFrameworkBenchmark PRIVATE MicroTelemetryFramework)
endif()

if(MTF_BUILD_EXTERNAL_COMPARISON_BENCHMARKS)
    include(FetchContent)
    # OpenTelemetry 1.9.1 still declares an old policy baseline. New CMake
    # versions require consumers to opt into the compatible policy floor.
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

    set(ENABLE_PULL OFF CACHE BOOL "" FORCE)
    set(ENABLE_PUSH OFF CACHE BOOL "" FORCE)
    set(ENABLE_COMPRESSION OFF CACHE BOOL "" FORCE)
    set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(OVERRIDE_CXX_STANDARD_FLAGS OFF CACHE BOOL "" FORCE)
    set(GENERATE_PKGCONFIG OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(mtf_prometheus_cpp
        GIT_REPOSITORY https://github.com/jupp0r/prometheus-cpp.git
        GIT_TAG e5fada43131d251e9c4786b04263ce98b6767ba5
        GIT_SHALLOW FALSE)
    FetchContent_MakeAvailable(mtf_prometheus_cpp)

    set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
    set(WITH_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(WITH_OTLP OFF CACHE BOOL "" FORCE)
    set(WITH_OTLP_GRPC OFF CACHE BOOL "" FORCE)
    set(WITH_OTLP_HTTP OFF CACHE BOOL "" FORCE)
    set(WITH_ZIPKIN OFF CACHE BOOL "" FORCE)
    set(WITH_PROMETHEUS OFF CACHE BOOL "" FORCE)
    set(WITH_BENCHMARK OFF CACHE BOOL "" FORCE)
    set(OPENTELEMETRY_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(mtf_opentelemetry_cpp
        GIT_REPOSITORY https://github.com/open-telemetry/opentelemetry-cpp.git
        GIT_TAG 770fce3c4095f6dc852fd80fb0810936b723be9a
        GIT_SHALLOW FALSE)
    FetchContent_MakeAvailable(mtf_opentelemetry_cpp)

    add_executable(MicroTelemetryFrameworkExternalBenchmark
        benchmarks/mtf_external_comparison.cpp)
    target_link_libraries(MicroTelemetryFrameworkExternalBenchmark PRIVATE
        MicroTelemetryFramework prometheus-cpp::core opentelemetry_metrics)
endif()

if(MTF_BUILD_ECOSYSTEM_BENCHMARKS)
    include(FetchContent)
    if(POLICY CMP0169)
        cmake_policy(PUSH)
        cmake_policy(SET CMP0169 OLD)
    endif()
    FetchContent_Declare(mtf_mef
        GIT_REPOSITORY https://github.com/R3zeProjects/MicroErrorFramework.git
        GIT_TAG 8c718fc49c65c82d7451434194b024b8e0197dec
        GIT_SHALLOW FALSE)
    FetchContent_Declare(mtf_mpf
        GIT_REPOSITORY https://github.com/R3zeProjects/MicroPersistenceFramework.git
        GIT_TAG fd6836abb96dacfbcfb4cb363edaf264b1442d64
        GIT_SHALLOW FALSE)
    FetchContent_GetProperties(mtf_mef)
    if(NOT mtf_mef_POPULATED)
        FetchContent_Populate(mtf_mef)
    endif()
    FetchContent_GetProperties(mtf_mpf)
    if(NOT mtf_mpf_POPULATED)
        FetchContent_Populate(mtf_mpf)
    endif()
    if(POLICY CMP0169)
        cmake_policy(POP)
    endif()

    set(MEF_CONTRACTS_SOURCE_DIR "${MTF_CONTRACTS_SOURCE_DIR}" CACHE PATH "" FORCE)
    set(MPF_MEF_SOURCE_DIR "${mtf_mef_SOURCE_DIR}" CACHE PATH "" FORCE)
    set(MPF_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(MPF_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
    set(MPF_BUILD_EXTERNAL_COMPARISON_BENCHMARKS OFF CACHE BOOL "" FORCE)
    set(_mtf_saved_build_testing "${BUILD_TESTING}")
    set(BUILD_TESTING OFF)
    add_subdirectory("${mtf_mpf_SOURCE_DIR}"
                     "${CMAKE_CURRENT_BINARY_DIR}/ecosystem/mpf" EXCLUDE_FROM_ALL)
    set(BUILD_TESTING "${_mtf_saved_build_testing}")
    unset(_mtf_saved_build_testing)

    add_executable(MicroTelemetryFrameworkEcosystemBenchmark
        benchmarks/mtf_ecosystem_benchmark.cpp)
    target_link_libraries(MicroTelemetryFrameworkEcosystemBenchmark PRIVATE
        MicroTelemetryFramework vosp::vosp vosp::persistence)
endif()

install(TARGETS MicroTelemetryFramework EXPORT mtfTargets)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES LICENSE DESTINATION
    ${CMAKE_INSTALL_DATADIR}/licenses/MicroTelemetryFramework)
install(EXPORT mtfTargets FILE mtfTargets.cmake NAMESPACE vosp::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/mtf)
configure_package_config_file(cmake/mtfConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/mtfConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/mtf)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/mtfConfigVersion.cmake
    VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/mtfConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/mtfConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/mtf)
