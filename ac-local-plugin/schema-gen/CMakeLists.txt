# Copyright (c) Alpaca Core
# SPDX-License-Identifier: MIT
#
add_executable(ac-whisper.cpp-schema-gen)
target_sources(ac-whisper.cpp-schema-gen PRIVATE
    schema-gen.cpp
)
target_link_libraries(ac-whisper.cpp-schema-gen PRIVATE
    ac::whisper.cpp-schema
    ac::local
)
