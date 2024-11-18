// Copyright (c) Alpaca Core
// SPDX-License-Identifier: MIT
//
#pragma once
#include "export.h"

namespace ac::local {
class ModelFactory;
AC_LOCAL_WHISPER_EXPORT void addWhisperInference(ModelFactory& provider);
}
