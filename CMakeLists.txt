cmake_minimum_required(VERSION 3.20)

project(pjson VERSION 0.1.0 LANGUAGES CXX)

include(FetchContent)

# @req CR-001 — pjson uses PersistMemoryManager as its persistent-storage dependency.
# Keep PMM pinned to the immutable revision that provides the storage contracts
# required by the active pjson phase. Do not silently track main. Phase C1 is
# temporarily pinned to the exact relocation-safe PMM PR head; before merge this
# must be replaced by the immutable merge commit that lands PMM #405.
# A commit SHA is intentionally fetched without GIT_SHALLOW: unlike a branch or
# tag it may later sit below the remote tip, and a depth-1 clone is not a stable
# way to guarantee that exact object remains available to FetchContent.
if(NOT TARGET pmm)
    set(_pjson_saved_build_testing "${BUILD_TESTING}")
    set(_pjson_saved_pmm_build_examples "${PMM_BUILD_EXAMPLES}")
    set(_pjson_saved_pmm_build_benchmarks "${PMM_BUILD_BENCHMARKS}")
    set(_pjson_saved_pmm_build_demo "${PMM_BUILD_DEMO}")

    set(BUILD_TESTING OFF)
    set(PMM_BUILD_EXAMPLES OFF)
    set(PMM_BUILD_BENCHMARKS OFF)
    set(PMM_BUILD_DEMO OFF)

    FetchContent_Declare(
        pmm
        GIT_REPOSITORY https://github.com/netkeep80/PersistMemoryManager.git
        GIT_TAG        e4e20058ed0f45bd463ef74a38e44c169effbbc9
    )
    FetchContent_MakeAvailable(pmm)

    set(BUILD_TESTING "${_pjson_saved_build_testing}")
    set(PMM_BUILD_EXAMPLES "${_pjson_saved_pmm_build_examples}")
    set(PMM_BUILD_BENCHMARKS "${_pjson_saved_pmm_build_benchmarks}")
    set(PMM_BUILD_DEMO "${_pjson_saved_pmm_build_demo}")
endif()

# @req NFR-001 — pjson is a header-only C++20 library.
add_library(pjson INTERFACE)
add_library(pjson::pjson ALIAS pjson)

target_compile_features(pjson INTERFACE cxx_std_20)
target_include_directories(pjson INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()
