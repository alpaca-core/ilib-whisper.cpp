# Copyright (c) Alpaca Core
# SPDX-License-Identifier: MIT
#
set(TARGET example-ac-whisper-basic)

add_executable(${TARGET} e-basic.cpp)
target_link_libraries(${TARGET} PRIVATE
    ac::whisper
    ac::jalog
    ac-test-data::whisper
    ac-dev::audio
)
set_target_properties(${TARGET} PROPERTIES FOLDER example)

CPMAddPackage(gh:alpaca-core/helper-imgui-sdl@1.0.0)
if(TARGET ac-dev::imgui-sdl-app)
    set(TARGET example-ac-whisper-gui)
    add_executable(${TARGET} e-gui.cpp)
    target_link_libraries(${TARGET} PRIVATE
        ac::whisper
        ac::jalog
        ac-test-data::whisper
        ac-dev::audio
        ac-dev::imgui-sdl-app
    )
    set_target_properties(${TARGET} PROPERTIES FOLDER example)
endif()
