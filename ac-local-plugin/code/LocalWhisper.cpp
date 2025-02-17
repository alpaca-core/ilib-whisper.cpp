// Copyright (c) Alpaca Core
// SPDX-License-Identifier: MIT
//
#include <ac/whisper/Instance.hpp>
#include <ac/whisper/Init.hpp>
#include <ac/whisper/Model.hpp>

#include <ac/local/Provider.hpp>

#include <ac/schema/WhisperCpp.hpp>
#include <ac/schema/OpDispatchHelpers.hpp>

#include <ac/frameio/SessionCoro.hpp>
#include <ac/FrameUtil.hpp>

#include <astl/move.hpp>
#include <astl/move_capture.hpp>
#include <astl/iile.h>
#include <astl/throw_stdex.hpp>
#include <astl/workarounds.h>

#include "aclp-whisper-version.h"
#include "aclp-whisper-interface.hpp"

namespace ac::local {

namespace sc = schema::whisper;
using namespace ac::frameio;

struct BasicRunner {
    schema::OpDispatcherData m_dispatcherData;

    Frame dispatch(Frame& f) {
        try {
            auto ret = m_dispatcherData.dispatch(f.op, std::move(f.data));
            if (!ret) {
                throw_ex{} << "dummy: unknown op: " << f.op;
            }
            return {f.op, *ret};
        }
        catch (std::exception& e) {
            return {"error", e.what()};
        }
    }
};

namespace {

SessionCoro<void> Whisper_runInstance(coro::Io io, std::unique_ptr<whisper::Instance> instance) {
    using Schema = sc::StateInstance;

    struct Runner : public BasicRunner {
        whisper::Instance& m_instance;

        Runner(whisper::Instance& instance)
            : m_instance(instance)
        {
            schema::registerHandlers<Schema::Ops>(m_dispatcherData, *this);
        }

        Schema::OpTranscribe::Return on(Schema::OpTranscribe, Schema::OpTranscribe::Params&& params) {
            const auto& pcmf32 = params.audio.value();

            return {
                .text = m_instance.transcribe(pcmf32)
            };
        }
    };

    co_await io.pushFrame(Frame_stateChange(Schema::id));

    Runner runner(*instance);
    while (true)
    {
        auto f = co_await io.pollFrame();
        co_await io.pushFrame(runner.dispatch(f.frame));
    }
}

SessionCoro<void> Whisper_runModel(coro::Io io, std::unique_ptr<whisper::Model> model) {
    using Schema = sc::StateModelLoaded;

    struct Runner : public BasicRunner {
        Runner(whisper::Model& model)
            : lmodel(model)
        {
            schema::registerHandlers<Schema::Ops>(m_dispatcherData, *this);
        }

        whisper::Model& lmodel;
        std::unique_ptr<whisper::Instance> instance;

        static whisper::Instance::InitParams InstanceParams_fromSchema(sc::StateModelLoaded::OpStartInstance::Params& params) {
            whisper::Instance::InitParams ret;
            if (params.sampler == "greedy") {
                ret.samplingStrategy = whisper::Instance::InitParams::GREEDY;
            } else if (params.sampler == "beam_search") {
                ret.samplingStrategy = whisper::Instance::InitParams::BEAM_SEARCH;
            } else {
                throw_ex{} << "whisper: unknown sampler type: " << params.sampler.value();
                MSVC_WO_10766806();
            }
            return ret;
        }

        Schema::OpStartInstance::Return on(Schema::OpStartInstance, Schema::OpStartInstance::Params params) {
            instance = std::make_unique<whisper::Instance>(lmodel, InstanceParams_fromSchema(params));

            return {};
        }
    };

    co_await io.pushFrame(Frame_stateChange(Schema::id));

    Runner runner(*model);
    while (true)
    {
        auto f = co_await io.pollFrame();
        co_await io.pushFrame(runner.dispatch(f.frame));
        if (runner.instance) {
            co_await Whisper_runInstance(io, std::move(runner.instance));
        }
    }
}

SessionCoro<void> Whisper_runSession() {
    using Schema = sc::StateInitial;

    struct Runner : public BasicRunner {
        Runner() {
            schema::registerHandlers<Schema::Ops>(m_dispatcherData, *this);
        }

        std::unique_ptr<whisper::Model> model;

        static whisper::Model::Params ModelParams_fromSchema(sc::StateInitial::OpLoadModel::Params schemaParams) {
            whisper::Model::Params ret;
            ret.gpu = schemaParams.useGpu.valueOr(true);
            return ret;
        }

        Schema::OpLoadModel::Return on(Schema::OpLoadModel, Schema::OpLoadModel::Params params) {
            auto bin = params.binPath.valueOr("");
            auto lparams = ModelParams_fromSchema(params);

            model = std::make_unique<whisper::Model>(bin.c_str(), astl::move(lparams));

            return {};
        }
    };

    try {
        auto io = co_await coro::Io{};

        co_await io.pushFrame(Frame_stateChange(Schema::id));

        Runner runner;

        while (true)
        {
            auto f = co_await io.pollFrame();
            co_await io.pushFrame(runner.dispatch(f.frame));
            if (runner.model) {
                co_await Whisper_runModel(io, std::move(runner.model));
            }
        }
    }
    catch (std::exception& e) {
        co_return;
    }
}

class WhisperProvider final : public Provider {
public:
    virtual const Info& info() const noexcept override {
        static Info i = {
            .name = "ac whisper.cpp",
            .vendor = "Alpaca Core",
        };
        return i;
    }

    virtual frameio::SessionHandlerPtr createSessionHandler(std::string_view) override {
        return CoroSessionHandler::create(Whisper_runSession());
    }
};
}

} // namespace ac::local

namespace ac::whisper {

void init() {
    initLibrary();
}

std::vector<ac::local::ProviderPtr> getProviders() {
    std::vector<ac::local::ProviderPtr> ret;
    ret.push_back(std::make_unique<local::WhisperProvider>());
    return ret;
}

local::PluginInterface getPluginInterface() {
    return {
        .label = "ac whisper.cpp",
        .desc = "whisper.cpp plugin for ac-local",
        .vendor = "Alpaca Core",
        .version = astl::version{
            ACLP_whisper_VERSION_MAJOR, ACLP_whisper_VERSION_MINOR, ACLP_whisper_VERSION_PATCH
        },
        .init = init,
        .getProviders = getProviders,
    };
}

} // namespace ac::whisper

