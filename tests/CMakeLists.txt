cmake_minimum_required(VERSION 3.31...4.4.2)
project(discusy_tests LANGUAGES C CXX)

include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/catch2.git
    GIT_TAG        "${DISCUSY_CATCH2_VERSION}"
    EXCLUDE_FROM_ALL
    SYSTEM
)
FetchContent_MakeAvailable(Catch2)

set(TEST_SOURCES
    unit/test_rate_limiter.cpp
    unit/test_cache.cpp
    unit/test_callbacks.cpp
    unit/test_multipart.cpp
    unit/test_snowflake.cpp
    unit/test_zstd.cpp
)

add_executable(discusy_unit_tests ${TEST_SOURCES})

target_link_libraries(discusy_unit_tests PRIVATE
    discusy::discusy
    discusy_exe_options
    Catch2::Catch2WithMain
)

if(WIN32)
    file(GLOB DISCUSY_TEST_OPENSSL_DLLS
        "${PROJECT_SOURCE_DIR}/3rdparty/openssl/bin/*.dll"
        "${DISCUSY_OPENSSL_BUILD_DIR}/bin/*.dll"
    )

    if(DISCUSY_TEST_OPENSSL_DLLS)
        file(COPY ${DISCUSY_TEST_OPENSSL_DLLS} DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")

        add_custom_command(TARGET discusy_unit_tests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${DISCUSY_TEST_OPENSSL_DLLS}
                "$<TARGET_FILE_DIR:discusy_unit_tests>"
            COMMENT "Copying OpenSSL runtime DLLs for test discovery..."
        )
    endif()
endif()

include(Catch)
catch_discover_tests(discusy_unit_tests)
