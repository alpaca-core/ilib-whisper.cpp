// Copyright (c) Alpaca Core
// SPDX-License-Identifier: MIT
//
#pragma once
#include <ac/schema/Field.hpp>
#include <ac/Dict.hpp>
#include <vector>
#include <string>
#include <tuple>

namespace ac::schema {

struct WhisperCppInterface {
    static inline constexpr std::string_view id = "whisper.cpp";
    static inline constexpr std::string_view description = "ilib-whisper.cpp-specific interface";

    struct OpTranscribe {
        static inline constexpr std::string_view id = "transcribe";
        static inline constexpr std::string_view description = "Run the whisper.cpp inference and produce some output";

        struct Params {
            Field<ac::Blob> audio;

            template <typename Visitor>
            void visitFields(Visitor& v) {
                v(audio, "audio_binary_mono", "Audio data to transcribe");
            }
        };

        struct Return {
            Field<std::string> result;

            template <typename Visitor>
            void visitFields(Visitor& v) {
                v(result, "result", "Transcription of audio");
            }
        };
    };

    using Ops = std::tuple<OpTranscribe>;
};

struct WhisperCppProvider {
    static inline constexpr std::string_view id = "whisper.cpp";
    static inline constexpr std::string_view description = "Inference based on our fork of https://github.com/ggerganov/whisper.cpp";

    using Params = nullptr_t;

    struct InstanceGeneral {
        static inline constexpr std::string_view id = "general";
        static inline constexpr std::string_view description = "General instance";

        using Params = nullptr_t;

        using Interfaces = std::tuple<WhisperCppInterface>;
    };

    using Instances = std::tuple<InstanceGeneral>;
};

} // namespace ac::local::schema
