# Copyright (c) Alpaca Core
# SPDX-License-Identifier: MIT
#
add_ac_local_plugin(
    NAME whisper
    SOURCES
        LocalWhisper.cpp
    SCHEMAS
        whisper-schema.yml
    LIBRARIES
        ac::whisper
)
