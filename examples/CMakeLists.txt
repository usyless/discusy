cmake_minimum_required(VERSION 3.31)
project(discusy_examples LANGUAGES CXX)

add_subdirectory(slash_commands)
add_subdirectory(components)
add_subdirectory(components_with_when)
if(DISCUSY_ENABLE_VOICE)
    add_subdirectory(voice)
endif()
add_subdirectory(receiving_events)
add_subdirectory(updating_presences)
add_subdirectory(multithreading)
add_subdirectory(using_completion_tokens)
add_subdirectory(random_image_bot)

if(TARGET discusy_pch)
    foreach(_discusy_example IN ITEMS
        example_slash_commands
        example_components
        example_components_with_when
        example_voice
        example_receiving_events
        example_updating_presences
        example_multithreading
        example_using_completion_tokens
        example_random_image_bot
    )
        if(TARGET ${_discusy_example})
            if(MSVC)
                target_compile_definitions(${_discusy_example} PRIVATE _MBCS)
            endif()
            target_precompile_headers(${_discusy_example} REUSE_FROM discusy_pch)
        endif()
    endforeach()
endif()
