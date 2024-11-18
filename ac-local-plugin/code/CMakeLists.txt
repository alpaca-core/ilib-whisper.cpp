# Copyright (c) Alpaca Core
# SPDX-License-Identifier: MIT
#
add_ac_local_plugin(
    NAME whisper
    SOURCES
        LocalWhisper.cpp
    LIBRARIES
        ac::whisper
)
