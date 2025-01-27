# Copyright (c) Alpaca Core
# SPDX-License-Identifier: MIT
#
add_library(ac-whisper.cpp-schema INTERFACE)
add_library(ac::whisper.cpp-schema ALIAS ac-whisper.cpp-schema)
target_link_libraries(ac-whisper.cpp-schema INTERFACE ac::frame)
target_include_directories(ac-whisper.cpp-schema INTERFACE .)
