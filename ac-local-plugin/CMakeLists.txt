# Copyright (c) Alpaca Core
# SPDX-License-Identifier: MIT
#
add_subdirectory(schema)
add_subdirectory(schema-gen)
add_subdirectory(code)

if(AC_WHISPER_BUILD_EXAMPLES)
    add_subdirectory(example)
endif()
