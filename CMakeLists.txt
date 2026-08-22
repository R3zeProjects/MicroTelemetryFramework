cmake_minimum_required(VERSION 3.25)

project(MicroTelemetryFramework VERSION 0.1.0 LANGUAGES CXX)

set(MICRO_TELEMETRY_FRAMEWORK_PRERELEASE "beta")

include(CTest)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
find_package(Threads REQUIRED)

option(MTF_BUILD_EXAMPLES "Build MTF examples" ON)
option(MTF_BUILD_BENCHMARKS "Build opt-in MTF benchmarks" OFF)
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
                GIT_TAG b9f78d3f529097ac1dae963b06274a6110b39c1a
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

if(MSVC)
    target_compile_options(MicroTelemetryFramework INTERFACE /W4 /permissive-)
else()
    target_compile_options(MicroTelemetryFramework INTERFACE -Wall -Wextra -Wpedantic)
endif()

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
endif()

if(MTF_BUILD_BENCHMARKS)
    add_executable(MicroTelemetryFrameworkBenchmark benchmarks/mtf_benchmark.cpp)
    target_link_libraries(MicroTelemetryFrameworkBenchmark PRIVATE MicroTelemetryFramework)
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
