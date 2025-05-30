// Copyright (c) Alpaca Core
// SPDX-License-Identifier: MIT
//
#pragma once
#include <ac/schema/Field.hpp>
#include <ac/schema/Progress.hpp>
#include <ac/schema/StateChange.hpp>
#include <ac/Dict.hpp>
#include <vector>
#include <string>
#include <tuple>

namespace ac::schema {

inline namespace whisper {

struct StateWhisper {
    static constexpr auto id = "whisper.cpp";
    static constexpr auto desc = "Initial state";

    struct OpLoadModel {
        static constexpr auto id = "load-model";
        static constexpr auto desc = "Load the whisper.cpp model";

        struct Params{
            Field<std::string> binPath = std::nullopt;
            Field<bool> useGpu = Default(true);

            template <typename Visitor>
            void visitFields(Visitor& v) {
                v(binPath, "binPath", "Path to the file with model data.");
                v(useGpu, "useGpu", "Whether to use GPU for inference");
            }
        };

        using Return = StateChange;

        using Ins = std::tuple<>;
        using Outs = std::tuple<sys::Progress>;
    };

    using Ops = std::tuple<OpLoadModel>;
};

struct StateModelLoaded {
    static constexpr auto id = "model-loaded";
    static constexpr auto desc = "Model loaded state";

    struct OpStartInstance {
        static constexpr auto id = "start-instance";
        static constexpr auto desc = "Start a new instance of the whisper.cpp model";

        struct Params {
            Field<std::string> sampler = Default("greedy");

            template <typename Visitor>
            void visitFields(Visitor& v) {
                v(sampler, "sampler_type", "Type of the sampler to use. Options[]: greedy, beam_search");
            }
        };

        using Return = StateChange;
    };

    using Ops = std::tuple<OpStartInstance>;
};

struct StateInstance {
    static constexpr auto id = "instance";
    static constexpr auto desc = "Instance state";

    struct OpTranscribe {
        static inline constexpr std::string_view id = "transcribe";
        static inline constexpr std::string_view desc = "Run the whisper.cpp inference and produce some output.";

        struct Params {
            Field<std::vector<float>> audio;

            template <typename Visitor>
            void visitFields(Visitor& v) {
                v(audio, "audio_binary_mono", "Audio data to transcribe");
            }
        };

        struct Return {
            Field<std::string> text;

            template <typename Visitor>
            void visitFields(Visitor& v) {
                v(text, "text", "Transcription of audio");
            }
        };

        using Type = Return;
    };

    using Ops = std::tuple<OpTranscribe>;
    using Ins = std::tuple<>;
    using Outs = std::tuple<>;
};

struct Interface {
    static inline constexpr std::string_view id = "whisper.cpp";
    static inline constexpr std::string_view desc = "Inference based on our fork of https://github.com/ggerganov/whisper.cpp";

    using States = std::tuple<StateWhisper>;
};

} // namespace whisper

} // namespace ac::local::schema
