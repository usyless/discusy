cmake_minimum_required(VERSION 3.31...4.4.2)

set(CMAKE_EXPORT_NO_PACKAGE_REGISTRY ON CACHE BOOL "" FORCE)
set(CMAKE_FIND_USE_PACKAGE_REGISTRY OFF CACHE BOOL "" FORCE)

project(
    discusy
    VERSION 1.0.0
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(PROJECT_IS_TOP_LEVEL)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

include(CMakeDependentOption)
include(FetchContent)

##### Options

# Core Features & Build Configuration
option(DISCUSY_USE_MODULES "Enable C++20 Modules support for discusy (NOT WORKING)" OFF)
cmake_dependent_option(DISCUSY_ENABLE_PCH "Enable PCH for quicker compilation" ON "NOT DISCUSY_USE_MODULES" OFF)
option(DISCUSY_LOGGING_ENABLED "Enable logging in release mode" OFF)
option(DISCUSY_USE_MIMALLOC "Replace default allocator with mimalloc" OFF)
option(DISCUSY_USE_INTERPROCEDURAL_OPTIMIZATION "Enable FLTO/LTCG" ON)
option(DISCUSY_USE_CORO_HTTP_CLIENT "Use co_composed coroutine-based HTTP client instead of callback-based HTTP client (EXPERIMENTAL - MIGHT HAVE SAFETY ISSUES)" OFF)
cmake_dependent_option(DISCUSY_USE_STATIC_MSVCRT
    "Use the static release/debug MSVCRT - or set the flag yourself before including the project" OFF
    "MSVC" OFF
)
option(DISCUSY_NO_CACHES "Disable the caches of channels/users/guilds/etc from existing at all" ON)

# Voice & Audio Features
option(DISCUSY_ENABLE_VOICE "Enable voice support" OFF)
cmake_dependent_option(DISCUSY_NO_DEDICATED_AUDIO_THREAD
    "Disable the dedicated voice threads" OFF
    "DISCUSY_ENABLE_VOICE" OFF
)

# Platform-Specific: Linux & io_uring
cmake_dependent_option(DISCUSY_ENABLE_IO_URING
    "Enable io_uring if supported - for async file operations" OFF
    "LINUX" OFF
)
cmake_dependent_option(DISCUSY_ENABLE_IO_URING_NETWORKING
    "Enable io_uring if supported - for networking too - disables epoll" OFF
    "LINUX;DISCUSY_ENABLE_IO_URING" OFF
)
cmake_dependent_option(DISCUSY_STATIC_LIBURING
    "Enable static liburing if io_uring supported" OFF
    "LINUX;DISCUSY_ENABLE_IO_URING" OFF
)

# Diagnostics & Sanitizers
option(DISCUSY_ENABLE_ASAN "Enable AddressSanitizer (ASan)" OFF)

cmake_dependent_option(DISCUSY_ENABLE_UBSAN
    "Enable UndefinedBehaviorSanitizer (UBSan)" OFF
    "NOT CMAKE_CXX_COMPILER_ID STREQUAL \"MSVC\"" OFF
)

cmake_dependent_option(DISCUSY_ENABLE_TSAN
    "Enable ThreadSanitizer (TSan)" OFF
    "NOT WIN32;NOT DISCUSY_ENABLE_ASAN;NOT DISCUSY_ENABLE_UBSAN" OFF
)

# Testing & Example Options
option(DISCUSY_BUILD_TESTS "Build discusy unit and integration tests" OFF)
option(DISCUSY_BUILD_EXAMPLES "Build discusy examples" OFF)

macro(set_tracked_cache_version VAR_NAME DEFAULT_VAL DOCSTRING)
    if(NOT "${${VAR_NAME}_LAST_DEFAULT}" STREQUAL "${DEFAULT_VAL}")
        set(${VAR_NAME} "${DEFAULT_VAL}" CACHE STRING "${DOCSTRING}" FORCE)
        set(${VAR_NAME}_LAST_DEFAULT "${DEFAULT_VAL}" CACHE INTERNAL "Tracks default change for ${VAR_NAME}")
    else()
        set(${VAR_NAME} "${DEFAULT_VAL}" CACHE STRING "${DOCSTRING}")
    endif()
endmacro()

# Dependency Versions
set_tracked_cache_version(DISCUSY_USYLIBPP_VERSION "main" "Version (git tag/branch) to use for usylibpp")
set_tracked_cache_version(DISCUSY_ZSTD_VERSION "1.5.7" "Version (git tag/branch) to use for zstd - without the leading v")
set_tracked_cache_version(DISCUSY_BOOST_VERSION "1.92.0" "Version (git tag/branch) to use for boost")
set_tracked_cache_version(DISCUSY_GLAZE_VERSION "v8.4.0" "Version (git tag/branch) to use for glaze")
set_tracked_cache_version(DISCUSY_OPENSSL_VERSION "4.0.1" "Version (git tag/branch) to use for openssl (UNUSED FOR NOW)")
set_tracked_cache_version(DISCUSY_LIBSODIUM_VERSION "1.0.22-RELEASE" "Version (git tag/branch) to use for libsodium")
set_tracked_cache_version(DISCUSY_NLOHMANN_JSON_VERSION "v3.12.0" "Version (git tag/branch) to use for nlohmann json")
set_tracked_cache_version(DISCUSY_MLSPP_VERSION "92aaa4134fa45ec39957a7c81a342401fba7feb2" "Version (git tag/branch) to use for mlspp")
set_tracked_cache_version(DISCUSY_LIBDAVE_VERSION "v1.2.0/cpp" "Version (git tag/branch) to use for libdave")
set_tracked_cache_version(DISCUSY_OPUS_VERSION "v1.6.1" "Version (git tag/branch) to use for opus")
set_tracked_cache_version(DISCUSY_MIMALLOC_VERSION "v3.5.3" "Version (git tag/branch) to use for mimalloc")
set_tracked_cache_version(DISCUSY_CATCH2_VERSION "v3.16.0" "Version (git tag/branch) to use for Catch2")

set(DISCUSY_EXTRA_BOOST_LIBS "" CACHE STRING "Additional Boost libraries to include")

# Status Reports
if(WIN32)
    message(STATUS "[${PROJECT_NAME}] Using static MSVCRT: ${DISCUSY_USE_STATIC_MSVCRT} (DISCUSY_USE_STATIC_MSVCRT)")
endif()
message(STATUS "[${PROJECT_NAME}] Using interprocedural optimization: ${DISCUSY_USE_INTERPROCEDURAL_OPTIMIZATION} (DISCUSY_USE_INTERPROCEDURAL_OPTIMIZATION)")
message(STATUS "[${PROJECT_NAME}] Using Modules: ${DISCUSY_USE_MODULES} (DISCUSY_USE_MODULES)")
message(STATUS "[${PROJECT_NAME}] Using PCH: ${DISCUSY_ENABLE_PCH} (DISCUSY_ENABLE_PCH)")
if(DISCUSY_LOGGING_ENABLED)
    message(STATUS "[${PROJECT_NAME}] Logging enabled! (DISCUSY_LOGGING_ENABLED)")
else()
    message(STATUS "[${PROJECT_NAME}] Logging depends on debug status! (DISCUSY_LOGGING_ENABLED)")
endif()
message(STATUS "[${PROJECT_NAME}] Voice support: ${DISCUSY_ENABLE_VOICE} (DISCUSY_ENABLE_VOICE)")
if(DISCUSY_ENABLE_VOICE)
    message(STATUS "[${PROJECT_NAME}] Dedicated voice thread disabled: ${DISCUSY_NO_DEDICATED_AUDIO_THREAD} (DISCUSY_NO_DEDICATED_AUDIO_THREAD)")
endif()
message(STATUS "[${PROJECT_NAME}] Using mimalloc: ${DISCUSY_USE_MIMALLOC} (DISCUSY_USE_MIMALLOC)")
message(STATUS "[${PROJECT_NAME}] Using Coro HTTP Client (EXPERIMENTAL - MIGHT HAVE SAFETY ISSUES): ${DISCUSY_USE_CORO_HTTP_CLIENT} (DISCUSY_USE_CORO_HTTP_CLIENT)")
message(STATUS "[${PROJECT_NAME}] Caches disabled: ${DISCUSY_NO_CACHES} (DISCUSY_NO_CACHES)")

message(STATUS "[${PROJECT_NAME}] Using ASan: ${DISCUSY_ENABLE_ASAN} (DISCUSY_ENABLE_ASAN)")

if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    message(STATUS "[${PROJECT_NAME}] Using UBSan: ${DISCUSY_ENABLE_UBSAN} (DISCUSY_ENABLE_UBSAN)")
    message(STATUS "[${PROJECT_NAME}] Using TSan: ${DISCUSY_ENABLE_TSAN} (DISCUSY_ENABLE_TSAN)")
endif()

message(STATUS "[${PROJECT_NAME}] Using usylibpp version: ${DISCUSY_USYLIBPP_VERSION} (DISCUSY_USYLIBPP_VERSION)")
message(STATUS "[${PROJECT_NAME}] Using zstd version: v${DISCUSY_ZSTD_VERSION} (DISCUSY_ZSTD_VERSION)")
message(STATUS "[${PROJECT_NAME}] Using boost version: ${DISCUSY_BOOST_VERSION} (DISCUSY_BOOST_VERSION)")
message(STATUS "[${PROJECT_NAME}] Using glaze version: ${DISCUSY_GLAZE_VERSION} (DISCUSY_GLAZE_VERSION)")
message(STATUS "[${PROJECT_NAME}] Build tests: ${DISCUSY_BUILD_TESTS} (DISCUSY_BUILD_TESTS)")
message(STATUS "[${PROJECT_NAME}] Build examples: ${DISCUSY_BUILD_EXAMPLES} (DISCUSY_BUILD_EXAMPLES)")
if(DISCUSY_BUILD_TESTS)
    message(STATUS "[${PROJECT_NAME}] Using Catch2 version: ${DISCUSY_CATCH2_VERSION} (DISCUSY_CATCH2_VERSION)")
endif()

if(DISCUSY_ENABLE_VOICE)
    message(STATUS "[${PROJECT_NAME}] Using libsodium version: ${DISCUSY_LIBSODIUM_VERSION} (DISCUSY_LIBSODIUM_VERSION)")
    message(STATUS "[${PROJECT_NAME}] Using nlohmann json version: ${DISCUSY_NLOHMANN_JSON_VERSION} (DISCUSY_NLOHMANN_JSON_VERSION)")
    message(STATUS "[${PROJECT_NAME}] Using mlspp version: ${DISCUSY_MLSPP_VERSION} (DISCUSY_MLSPP_VERSION)")
    message(STATUS "[${PROJECT_NAME}] Using libdave version: ${DISCUSY_LIBDAVE_VERSION} (DISCUSY_LIBDAVE_VERSION)")
    message(STATUS "[${PROJECT_NAME}] Using opus version: ${DISCUSY_OPUS_VERSION} (DISCUSY_OPUS_VERSION)")
endif()

if(DISCUSY_USE_MIMALLOC)
    message(STATUS "[${PROJECT_NAME}] Using mimalloc version: ${DISCUSY_MIMALLOC_VERSION} (DISCUSY_MIMALLOC_VERSION)")
endif()

if(DISCUSY_USE_STATIC_MSVCRT)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

if(DISCUSY_USE_INTERPROCEDURAL_OPTIMIZATION)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

if(DISCUSY_ENABLE_ASAN)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        add_compile_options(/fsanitize=address /UNDEBUG)
    else()
        add_compile_options(-fsanitize=address -fno-omit-frame-pointer -fno-sanitize-recover=all -UNDEBUG)
        add_link_options(-fsanitize=address -fno-sanitize-recover=all)
    endif()
endif()

if(DISCUSY_ENABLE_UBSAN)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        message(FATAL_ERROR "[${PROJECT_NAME}] UndefinedBehaviorSanitizer (UBSan) is not supported by MSVC (cl.exe). Use clang-cl, Clang, or GCC.")
    else()
        add_compile_options(-fsanitize=undefined -fno-omit-frame-pointer -fno-sanitize-recover=all -UNDEBUG)
        add_link_options(-fsanitize=undefined -fno-sanitize-recover=all)
    endif()
endif()

if(DISCUSY_ENABLE_TSAN)
    add_compile_options(-fsanitize=thread -fno-omit-frame-pointer -fno-sanitize-recover=all -UNDEBUG)
    add_link_options(-fsanitize=thread -fno-sanitize-recover=all)
endif()

# Target Setup: Main Library
set(DISCUSY_DEPS_DIR "${CMAKE_BINARY_DIR}/_deps")
set(LIBRARY_NAME "${PROJECT_NAME}_${PROJECT_NAME}")
set(DISCUSY_CXX_STD "cxx_std_23")

configure_file(
    "${PROJECT_SOURCE_DIR}/cmake/version.hpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/include/discusy/version.hpp"
    @ONLY
)

if(DISCUSY_USE_MODULES)
    add_library(${LIBRARY_NAME} STATIC)
    set(USYLIBPP_ENABLE_PCH OFF CACHE BOOL "" FORCE)
    set(DISCUSY_VISIBILITY "PUBLIC")

    target_compile_features(${LIBRARY_NAME} ${DISCUSY_VISIBILITY}
        "${DISCUSY_CXX_STD}"
    )

    target_compile_definitions(${LIBRARY_NAME} ${DISCUSY_VISIBILITY}
        DISCUSY_USE_MODULES
    )

    target_sources(${LIBRARY_NAME}
        ${DISCUSY_VISIBILITY}
            FILE_SET CXX_MODULES
            FILES "${PROJECT_SOURCE_DIR}/include/${PROJECT_NAME}/${PROJECT_NAME}.cppm"
    )
else()
    add_library(${LIBRARY_NAME} INTERFACE)
    set(DISCUSY_VISIBILITY "INTERFACE")

    target_compile_features(${LIBRARY_NAME} ${DISCUSY_VISIBILITY}
        "${DISCUSY_CXX_STD}"
    )

    file(GLOB_RECURSE DISCUSY_HEADERS CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/include/${PROJECT_NAME}/*.hpp")
    list(FILTER DISCUSY_HEADERS EXCLUDE REGEX "pch_top_level\\.hpp$")

    target_sources(${LIBRARY_NAME}
        ${DISCUSY_VISIBILITY}
            FILE_SET HEADERS
            BASE_DIRS "${PROJECT_SOURCE_DIR}/include"
            FILES ${DISCUSY_HEADERS}
    )

    if(DISCUSY_ENABLE_PCH)
        set(USYLIBPP_ENABLE_PCH ON CACHE BOOL "" FORCE)
        if(PROJECT_IS_TOP_LEVEL)
            target_precompile_headers(${LIBRARY_NAME} ${DISCUSY_VISIBILITY}
                "<${PROJECT_NAME}/pch_top_level.hpp>"
            )
        else()
            set(USYLIBPP_ENABLE_PCH OFF CACHE BOOL "" FORCE)
            target_precompile_headers(${LIBRARY_NAME} ${DISCUSY_VISIBILITY}
                "<${PROJECT_NAME}/${PROJECT_NAME}.hpp>"
            )
        endif()
    else()
        set(USYLIBPP_ENABLE_PCH OFF CACHE BOOL "" FORCE)
    endif()
endif()

add_library("${PROJECT_NAME}::${PROJECT_NAME}" ALIAS ${LIBRARY_NAME})

target_include_directories(${LIBRARY_NAME} ${DISCUSY_VISIBILITY}
    "${CMAKE_CURRENT_BINARY_DIR}/include"
)

target_compile_definitions(${LIBRARY_NAME} ${DISCUSY_VISIBILITY}
    $<$<OR:$<BOOL:${DISCUSY_LOGGING_ENABLED}>,$<CONFIG:Debug>>:DISCUSY_LOGGING>
    $<$<BOOL:${DISCUSY_USE_CORO_HTTP_CLIENT}>:DISCUSY_USE_CORO_HTTP_CLIENT>
)

# OpenSSL Configuration
if(MSVC)
    find_program(PERL_EXECUTABLE NAMES perl)
    find_program(NASM_EXECUTABLE NAMES nasm)

    if(NOT PERL_EXECUTABLE OR NOT NASM_EXECUTABLE)
        message(STATUS "[${PROJECT_NAME}] Not able to build openssl as missing strawberry perl and nasm on the command line! Using included openssl 4.0.1")
    else()
        message(STATUS "[${PROJECT_NAME}] Using openssl version: ${DISCUSY_OPENSSL_VERSION} (DISCUSY_OPENSSL_VERSION)")

        set(DISCUSY_OPENSSL_NAME "openssl-${DISCUSY_OPENSSL_VERSION}")
        set(DISCUSY_OPENSSL_URL "https://github.com/openssl/openssl/releases/download/${DISCUSY_OPENSSL_NAME}/${DISCUSY_OPENSSL_NAME}.tar.gz")
        set(DISCUSY_OPENSSL_DOWNLOAD_DEST "${DISCUSY_DEPS_DIR}/${DISCUSY_OPENSSL_NAME}.tar.gz")
        set(DISCUSY_OPENSSL_EXTRACT_DEST "${DISCUSY_DEPS_DIR}/openssl/${DISCUSY_OPENSSL_NAME}")
        set(DISCUSY_OPENSSL_BUILD_DIR "${DISCUSY_DEPS_DIR}/openssl/${DISCUSY_OPENSSL_NAME}-build")

        FetchContent_Declare(
            discusy_openssl_src
            URL "${DISCUSY_OPENSSL_URL}"
            SOURCE_DIR "${DISCUSY_OPENSSL_EXTRACT_DEST}"
        )
        FetchContent_MakeAvailable(discusy_openssl_src)

        if(NOT EXISTS "${DISCUSY_OPENSSL_BUILD_DIR}")
            file(MAKE_DIRECTORY "${DISCUSY_OPENSSL_BUILD_DIR}")
        endif()

        set(DISCUSY_OPENSSL_SENTINEL "${DISCUSY_OPENSSL_BUILD_DIR}/openssl_build_complete.marker")

        add_custom_command(
            OUTPUT "${DISCUSY_OPENSSL_SENTINEL}"
            COMMAND perl Configure VC-WIN64A enable-static-vcruntime no-comp no-deprecated no-apps no-legacy no-tests no-docs no-makedepend --prefix="${DISCUSY_OPENSSL_BUILD_DIR}" --openssldir="${DISCUSY_OPENSSL_BUILD_DIR}/openssl"
            COMMAND nmake
            COMMAND nmake install_sw
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${DISCUSY_OPENSSL_BUILD_DIR}/bin/*.dll" "${CMAKE_BINARY_DIR}"
            COMMAND ${CMAKE_COMMAND} -E touch "${DISCUSY_OPENSSL_SENTINEL}"
            WORKING_DIRECTORY "${DISCUSY_OPENSSL_EXTRACT_DEST}"
            COMMENT "[${PROJECT_NAME}] Configuring and compiling OpenSSL..."
            VERBATIM
        )

        add_custom_target(discusy_openssl DEPENDS "${DISCUSY_OPENSSL_SENTINEL}")

        target_link_libraries(${LIBRARY_NAME} ${DISCUSY_VISIBILITY}
            discusy_openssl
        )

        target_include_directories(${LIBRARY_NAME} ${DISCUSY_VISIBILITY}
            "${DISCUSY_OPENSSL_BUILD_DIR}/include"
        )

        target_link_directories(${LIBRARY_NAME} ${DISCUSY_VISIBILITY}
            "${DISCUSY_OPENSSL_BUILD_DIR}/lib"
            "${DISCUSY_OPENSSL_BUILD_DIR}/bin"
        )
    endif()
else()
    message(STATUS "[${PROJECT_NAME}] Not using MSVC therefore openssl must be sourced yourself!")
endif()

# Dependencies
if(NOT DEFINED USYLIBPP_ENABLE_WINDOWS)
    set(USYLIBPP_ENABLE_WINDOWS OFF CACHE BOOL "" FORCE)
endif()
if(NOT DEFINED USYLIBPP_ENABLE_LINUX)
    set(USYLIBPP_ENABLE_LINUX OFF CACHE BOOL "" FORCE)
endif()

FetchContent_Declare(
    usylibpp
    GIT_REPOSITORY https://github.com/usyless/usylibpp.git
    GIT_TAG "${DISCUSY_USYLIBPP_VERSION}"
    GIT_SHALLOW TRUE
    EXCLUDE_FROM_ALL
    SYSTEM
)
FetchContent_MakeAvailable(usylibpp)

set(ZSTD_BUILD_STATIC ON CACHE INTERNAL "" FORCE)
set(ZSTD_BUILD_SHARED OFF CACHE INTERNAL "" FORCE)
set(ZSTD_BUILD_PROGRAMS OFF CACHE INTERNAL "" FORCE)
set(ZSTD_BUILD_TESTS OFF CACHE INTERNAL "" FORCE)
set(ZSTD_LEGACY_SUPPORT OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    zstd
    URL "https://github.com/facebook/zstd/releases/download/v${DISCUSY_ZSTD_VERSION}/zstd-${DISCUSY_ZSTD_VERSION}.tar.gz"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR build/cmake
    EXCLUDE_FROM_ALL
    SYSTEM
)
FetchContent_MakeAvailable(zstd)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    Boost
    URL "https://github.com/boostorg/boost/releases/download/boost-${DISCUSY_BOOST_VERSION}/boost-${DISCUSY_BOOST_VERSION}-cmake.7z"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    EXCLUDE_FROM_ALL
    SYSTEM
)

set(BOOST_INCLUDE_LIBRARIES beast asio system url container)

if(DISCUSY_EXTRA_BOOST_LIBS)
    list(APPEND BOOST_INCLUDE_LIBRARIES ${DISCUSY_EXTRA_BOOST_LIBS})
    list(REMOVE_DUPLICATES BOOST_INCLUDE_LIBRARIES)
endif()

FetchContent_MakeAvailable(Boost)

FetchContent_Declare(
    glaze
    GIT_REPOSITORY https://github.com/stephenberry/glaze.git
    GIT_TAG "${DISCUSY_GLAZE_VERSION}"
    GIT_SHALLOW TRUE
    EXCLUDE_FROM_ALL
    SYSTEM
)
FetchContent_MakeAvailable(glaze)

target_link_libraries(${LIBRARY_NAME} ${DISCUSY_VISIBILITY}
    usylibpp::usylibpp
    glaze::glaze
    Boost::headers
    Boost::system
    Boost::beast
    Boost::asio
    Boost::url
    Boost::container
    libzstd_static
)

target_include_directories(${LIBRARY_NAME} ${DISCUSY_VISIBILITY}
    "${zstd_SOURCE_DIR}/lib"
)

target_compile_definitions(${LIBRARY_NAME} ${DISCUSY_VISIBILITY}
    BOOST_ASIO_HAS_OPENSSL
    ASIO_HAS_OPENSSL
    BOOST_ASIO_NO_DEPRECATED
)

if(WIN32 AND (NOT DEFINED OPENSSL_ROOT_DIR))
    set(OPENSSL_ROOT_DIR "${PROJECT_SOURCE_DIR}/3rdparty/openssl")
endif()

find_package(OpenSSL REQUIRED)

target_link_libraries(${LIBRARY_NAME} ${DISCUSY_VISIBILITY}
    OpenSSL::SSL
    OpenSSL::Crypto
)

if(WIN32)
    file(GLOB DISCUSY_ROOT_OPENSSL_DLLS "${PROJECT_SOURCE_DIR}/3rdparty/openssl/bin/*.dll")
    if(DISCUSY_ROOT_OPENSSL_DLLS)
        file(COPY ${DISCUSY_ROOT_OPENSSL_DLLS} DESTINATION "${CMAKE_BINARY_DIR}")
    endif()
endif()

# Platform-Specific & Sanitizer Configuration
if(WIN32)
    message(STATUS "[${PROJECT_NAME}] Using windows and as such using io completion ports!")

    # /bigobj fixed no lto build
    target_compile_options(${LIBRARY_NAME} ${DISCUSY_VISIBILITY}
        $<$<OR:$<CXX_COMPILER_ID:MSVC>,$<CXX_COMPILER_FRONTEND_VARIANT:MSVC>>:
            /bigobj
            /Zc:preprocessor
            /await:strict
        >
        $<$<CXX_COMPILER_ID:GNU>:
            -Wa,-mbig-obj
        >
    )

    target_link_libraries(${LIBRARY_NAME} ${DISCUSY_VISIBILITY}
        Crypt32
        ws2_32
        mswsock
        advapi32
    )

    target_compile_definitions(${LIBRARY_NAME} ${DISCUSY_VISIBILITY}
        WINVER=0x0A00
        _WIN32_WINNT=0x0A00
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
else()
    find_package(Threads REQUIRED)

    target_link_libraries(${LIBRARY_NAME} ${DISCUSY_VISIBILITY}
        Threads::Threads
    )

    if(DISCUSY_ENABLE_TSAN AND (DISCUSY_ENABLE_ASAN OR DISCUSY_ENABLE_UBSAN))
        message(FATAL_ERROR "[${PROJECT_NAME}] ThreadSanitizer (TSan) cannot be enabled at the same time as ASan/UBSan! Choose one.")
    endif()

    if(LINUX)
        if(DISCUSY_ENABLE_IO_URING AND (CMAKE_HOST_SYSTEM_VERSION MATCHES "^([0-9]+)\\.([0-9]+)"))
            set(KERNEL_MAJOR ${CMAKE_MATCH_1})
            set(KERNEL_MINOR ${CMAKE_MATCH_2})

            find_package(PkgConfig QUIET)
            if(PKG_CONFIG_FOUND)
                pkg_check_modules(LIBURING liburing QUIET IMPORTED_TARGET)
            endif()

            # Check if kernel is >= 5.10
            if((KERNEL_MAJOR GREATER 5) OR (KERNEL_MAJOR EQUAL 5 AND KERNEL_MINOR GREATER_EQUAL 10))
                if(LIBURING_FOUND OR TARGET PkgConfig::LIBURING)
                    message(STATUS "[${PROJECT_NAME}] Host Linux Kernel is ${KERNEL_MAJOR}.${KERNEL_MINOR} -> Enabling io_uring")

                    target_compile_definitions(${LIBRARY_NAME} ${DISCUSY_VISIBILITY}
                        BOOST_ASIO_HAS_IO_URING
                        ASIO_HAS_IO_URING
                    )

                    if(DISCUSY_ENABLE_IO_URING_NETWORKING)
                        message(WARNING "[${PROJECT_NAME}] Using io_uring for networking as well as file reads! Disabling epoll.")
                        target_compile_definitions(${LIBRARY_NAME} ${DISCUSY_VISIBILITY}
                            BOOST_ASIO_DISABLE_EPOLL
                            ASIO_DISABLE_EPOLL
                        )
                    endif()

                    if(DISCUSY_STATIC_LIBURING)
                        find_library(LIBURING_STATIC NAMES liburing.a REQUIRED)
                        target_link_libraries(${LIBRARY_NAME} ${DISCUSY_VISIBILITY}
                            "${LIBURING_STATIC}"
                        )
                    else()
                        if(TARGET PkgConfig::LIBURING)
                            target_link_libraries(${LIBRARY_NAME} ${DISCUSY_VISIBILITY}
                                PkgConfig::LIBURING
                            )
                        else()
                            find_library(LIBURING_PATH NAMES uring liburing REQUIRED)
                            target_link_libraries(${LIBRARY_NAME} ${DISCUSY_VISIBILITY}
                                "${LIBURING_PATH}"
                            )
                        endif()
                    endif()
                else()
                    message(STATUS "[${PROJECT_NAME}] System kernel supports io_uring but liburing not installed! Install the respective dev package for your OS to enable. Falling back to epoll.")
                endif()
            else()
                message(WARNING "[${PROJECT_NAME}] Kernel ${CMAKE_HOST_SYSTEM_VERSION} is too old for stable io_uring. Falling back to epoll. Will not be able to do async file reads.")
            endif()
        else()
            message(WARNING "[${PROJECT_NAME}] Using epoll (or whatever your platforms default is) as io_uring disabled (DISCUSY_ENABLE_IO_URING). Will not be able to do async file reads.")
        endif()
    endif()
endif()

# Voice Support Configuration
if(DISCUSY_ENABLE_VOICE)
    target_compile_definitions(${LIBRARY_NAME} ${DISCUSY_VISIBILITY}
        DISCUSY_VOICE
    )

    if(DISCUSY_NO_DEDICATED_AUDIO_THREAD)
        target_compile_definitions(${LIBRARY_NAME} ${DISCUSY_VISIBILITY}
            DISCUSY_NO_DEDICATED_AUDIO_THREAD
        )
    endif()

    add_subdirectory("${PROJECT_SOURCE_DIR}/3rdparty/libsodium")

    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG "${DISCUSY_NLOHMANN_JSON_VERSION}"
        FIND_PACKAGE_ARGS NAMES nlohmann_json
        EXCLUDE_FROM_ALL
        SYSTEM
    )
    FetchContent_MakeAvailable(nlohmann_json)

    set(MLS_CXX_NAMESPACE "mlspp" CACHE STRING "" FORCE)
    set(DISABLE_GREASE ON CACHE BOOL "" FORCE)

    FetchContent_Declare(
        mlspp
        GIT_REPOSITORY https://github.com/cisco/mlspp.git
        GIT_TAG "${DISCUSY_MLSPP_VERSION}"
        FIND_PACKAGE_ARGS NAMES MLSPP
        EXCLUDE_FROM_ALL
        SYSTEM
        GIT_SUBMODULES ""
    )
    FetchContent_MakeAvailable(mlspp)

    if(TARGET mlspp AND NOT TARGET MLSPP::mlspp)
        add_library(MLSPP::mlspp ALIAS mlspp)
    endif()

    FetchContent_Declare(
        libdave
        GIT_REPOSITORY https://github.com/discord/libdave.git
        GIT_TAG "${DISCUSY_LIBDAVE_VERSION}"
        SOURCE_SUBDIR cpp
        EXCLUDE_FROM_ALL
        SYSTEM
        GIT_SUBMODULES ""
    )
    FetchContent_MakeAvailable(libdave)

    target_compile_options(libdave PRIVATE
        $<$<CXX_COMPILER_ID:GNU>:-w>
    )

    FetchContent_Declare(
        libopus
        GIT_REPOSITORY https://github.com/xiph/opus.git
        GIT_TAG "${DISCUSY_OPUS_VERSION}"
        EXCLUDE_FROM_ALL
        SYSTEM
    )
    FetchContent_MakeAvailable(libopus)

    target_link_libraries(${LIBRARY_NAME} ${DISCUSY_VISIBILITY}
        sodium
        libdave
        MLSPP::mlspp
        Opus::opus
    )
endif()

# mimalloc Configuration
if(DISCUSY_USE_MIMALLOC)
    target_compile_definitions(${LIBRARY_NAME} ${DISCUSY_VISIBILITY}
        DISCUSY_USE_MIMALLOC
    )

    if(WIN32)
        if(NOT CMAKE_MSVC_RUNTIME_LIBRARY OR CMAKE_MSVC_RUNTIME_LIBRARY MATCHES "DLL")
            message(WARNING "[${PROJECT_NAME}] mimalloc static overloading on Windows requires the static CRT runtime (/MT). Currently CMAKE_MSVC_RUNTIME_LIBRARY is '${CMAKE_MSVC_RUNTIME_LIBRARY}' (defaults to /MD). Set CMAKE_MSVC_RUNTIME_LIBRARY before fetching ${PROJECT_NAME} to 'MultiThreaded' or 'MultiThreaded$<$<CONFIG:Debug>:Debug>' to ensure mimalloc takes precedence over the CRT.")
        endif()
    endif()

    set(MI_OVERRIDE ON CACHE BOOL "" FORCE)
    set(MI_BUILD_OBJECT ON CACHE BOOL "" FORCE)
    set(MI_BUILD_STATIC OFF CACHE BOOL "" FORCE)
    set(MI_BUILD_SHARED OFF CACHE BOOL "" FORCE)
    set(MI_BUILD_TESTS OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
        mimalloc
        GIT_REPOSITORY https://github.com/microsoft/mimalloc.git
        GIT_TAG "${DISCUSY_MIMALLOC_VERSION}"
        EXCLUDE_FROM_ALL
        SYSTEM
    )
    FetchContent_MakeAvailable(mimalloc)

    target_link_libraries(${LIBRARY_NAME} ${DISCUSY_VISIBILITY}
        mimalloc-obj
    )
endif()

if(DISCUSY_NO_CACHES)
    target_compile_definitions(${LIBRARY_NAME} ${DISCUSY_VISIBILITY}
        DISCUSY_NO_CACHES
    )
endif()

message(STATUS "[${PROJECT_NAME}] Finished configuring.")

# Testing
if(PROJECT_IS_TOP_LEVEL)
    add_library(discusy_exe_options INTERFACE)

    if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        target_compile_options(discusy_exe_options INTERFACE
            /EHsc
            /W4
            $<$<CONFIG:Debug>:
                $<$<NOT:$<BOOL:${DISCUSY_ENABLE_ASAN}>>:/RTC1>
                /Zi
                /Od
            >
            $<$<CONFIG:Release>:
                /O2
                /Ob2
                /Oi
                /Oy
                /Gy
            >
        )

        target_link_options(discusy_exe_options INTERFACE
            $<$<CONFIG:Debug>:
                "/DEBUG"
            >
            $<$<CONFIG:Release>:
                "/OPT:ICF"
                "/OPT:REF"
            >
        )
    elseif(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
        target_compile_options(discusy_exe_options INTERFACE
            /EHsc
            /W4
            $<$<CONFIG:Debug>:
                /Zi
                /Od
            >
            $<$<CONFIG:Release>:
                /O2
                /Ob2
                /Oi
                /Oy
                /Gy
            >
        )

        target_link_options(discusy_exe_options INTERFACE
            $<$<CONFIG:Debug>:
                "/DEBUG"
            >
            $<$<CONFIG:Release>:
                "/OPT:ICF"
                "/OPT:REF"
            >
        )
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(discusy_exe_options INTERFACE
            $<$<CONFIG:Debug>:
                -g
                -O0
            >
            $<$<CONFIG:Release>:
                -O3
                -ffunction-sections
                -fdata-sections
                -fomit-frame-pointer
            >
        )

        target_link_options(discusy_exe_options INTERFACE
            $<$<CONFIG:Release>:
                $<$<NOT:$<PLATFORM_ID:Darwin,Windows>>:-Wl,--gc-sections -s>
                $<$<PLATFORM_ID:Windows>:-Wl,--gc-sections>
            >
        )
    endif()

    set(TEST_LIBRARY_NAME "${PROJECT_NAME}_test")
    add_executable(${TEST_LIBRARY_NAME}
        "${PROJECT_SOURCE_DIR}/src/test.cpp"
    )

    target_link_libraries(${TEST_LIBRARY_NAME} PRIVATE
        discusy::discusy
        discusy_exe_options
    )

    set(HTTP_CLIENT_TEST_LIBRARY_NAME "${PROJECT_NAME}_http_client_test")
    add_executable(${HTTP_CLIENT_TEST_LIBRARY_NAME}
        "${PROJECT_SOURCE_DIR}/src/http_client_test.cpp"
    )

    target_link_libraries(${HTTP_CLIENT_TEST_LIBRARY_NAME} PRIVATE
        discusy::discusy
        discusy_exe_options
    )

    if(WIN32)
        file(GLOB DISCUSY_MAIN_OPENSSL_DLLS
            "${PROJECT_SOURCE_DIR}/3rdparty/openssl/bin/*.dll"
            "${DISCUSY_OPENSSL_BUILD_DIR}/bin/*.dll"
        )
        if(DISCUSY_MAIN_OPENSSL_DLLS)
            add_custom_command(TARGET ${TEST_LIBRARY_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${DISCUSY_MAIN_OPENSSL_DLLS}
                    "$<TARGET_FILE_DIR:${TEST_LIBRARY_NAME}>"
                COMMENT "Copying OpenSSL runtime DLLs to ${TEST_LIBRARY_NAME} output dir..."
            )
            add_custom_command(TARGET ${HTTP_CLIENT_TEST_LIBRARY_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${DISCUSY_MAIN_OPENSSL_DLLS}
                    "$<TARGET_FILE_DIR:${HTTP_CLIENT_TEST_LIBRARY_NAME}>"
                COMMENT "Copying OpenSSL runtime DLLs to ${HTTP_CLIENT_TEST_LIBRARY_NAME} output dir..."
            )
        endif()
    endif()

    if(DISCUSY_BUILD_TESTS)
        enable_testing()
        add_subdirectory(tests)
    endif()

    if(DISCUSY_BUILD_EXAMPLES)
        add_subdirectory(examples)
    endif()
endif()
