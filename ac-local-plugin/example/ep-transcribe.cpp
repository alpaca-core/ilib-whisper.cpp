// Copyright (c) Alpaca Core
// SPDX-License-Identifier: MIT
//
#include <ac/local/Lib.hpp>
#include <ac/local/DefaultBackend.hpp>
#include <ac/schema/BlockingIoHelper.hpp>
#include <ac/schema/FrameHelpers.hpp>

#include <ac/schema/WhisperCpp.hpp>

#include <ac/jalog/Instance.hpp>
#include <ac/jalog/sinks/DefaultSink.hpp>

#include <iostream>

#include "ac-test-data-whisper-dir.h"
#include "aclp-whisper-info.h"

#include <ac-audio.hpp>

namespace schema = ac::schema::whisper;

int main() try {
    ac::jalog::Instance jl;
    jl.setup().add<ac::jalog::sinks::DefaultSink>();

    ac::local::Lib::loadPlugin(ACLP_whisper_PLUGIN_FILE);

    ac::local::DefaultBackend backend;
    ac::schema::BlockingIoHelper whisper(backend.connect("whisper.cpp", {}));

    auto sid = whisper.poll<ac::schema::StateChange>();
    std::cout << "Initial state: " << sid << '\n';

    for (auto x : whisper.stream<schema::StateWhisper::OpLoadModel>({
        .binPath = AC_TEST_DATA_WHISPER_DIR "/whisper-base.en-f16.bin"
    })) {
        std::cout << "Model loaded: " << x.tag.value() << " " << x.progress.value() << '\n';
    }

    sid = whisper.call<schema::StateModelLoaded::OpStartInstance>({
        .sampler = "greedy"
    });
    std::cout << "Instance started: " << sid << '\n';

    std::string audioFile = AC_TEST_DATA_WHISPER_DIR "/as-she-sat.wav";
    auto pcmf32 = ac::audio::loadWavF32Mono(audioFile);

    std::cout << "Local-whisper: Transcribing the audio [" << audioFile << "]: \n\n";

    auto result = whisper.call<schema::StateInstance::OpTranscribe>({
        .audio = std::move(pcmf32)
    });

    std::cout << result.text.value() << '\n';

    return 0;
}
catch (std::exception& e) {
    std::cerr << "exception: " << e.what() << "\n";
    return 1;
}
