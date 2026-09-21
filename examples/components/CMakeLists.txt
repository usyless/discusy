cmake_minimum_required(VERSION 3.31)
project(example_components LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)

# Link to discusy from parent repository if available, or fetch via FetchContent
if(NOT TARGET discusy::discusy)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../CMakeLists.txt")
        add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../.." "${CMAKE_CURRENT_BINARY_DIR}/discusy_build")
    else()
        include(FetchContent)
        FetchContent_Declare(
            discusy
            GIT_REPOSITORY https://github.com/usyless/discusy.git
            GIT_TAG main
        )
        FetchContent_MakeAvailable(discusy)
    endif()
endif()

add_executable(example_components main.cpp)

target_link_libraries(example_components PRIVATE
    discusy::discusy
)

if(TARGET discusy_exe_options)
    target_link_libraries(example_components PRIVATE discusy_exe_options)
endif()
