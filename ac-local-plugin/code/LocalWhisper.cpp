// Copyright (c) Alpaca Core
// SPDX-License-Identifier: MIT
//
#include <ac/whisper/Instance.hpp>
#include <ac/whisper/Init.hpp>
#include <ac/whisper/Model.hpp>

#include <ac/local/Service.hpp>
#include <ac/local/ServiceFactory.hpp>
#include <ac/local/ServiceInfo.hpp>
#include <ac/local/Backend.hpp>
#include <ac/local/BackendWorkerStrand.hpp>

#include <ac/schema/WhisperCpp.hpp>
#include <ac/schema/OpDispatchHelpers.hpp>

#include <ac/FrameUtil.hpp>
#include <ac/frameio/IoEndpoint.hpp>

#include <ac/xec/coro.hpp>
#include <ac/io/exception.hpp>

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
        catch (io::stream_closed_error&) {
            throw;
        }
        catch (std::exception& e) {
            return {"error", e.what()};
        }
    }
};

namespace {

xec::coro<void> Whisper_runInstance(IoEndpoint& io, std::unique_ptr<whisper::Instance> instance) {
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

    co_await io.push(Frame_stateChange(Schema::id));

    Runner runner(*instance);
    while (true)
    {
        auto f = co_await io.poll();
        co_await io.push(runner.dispatch(*f));
    }
}

xec::coro<void> Whisper_runModel(IoEndpoint& io, std::unique_ptr<whisper::Model> model) {
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

    co_await io.push(Frame_stateChange(Schema::id));

    Runner runner(*model);
    while (true) {
        auto f = co_await io.poll();
        co_await io.push(runner.dispatch(*f));
        if (runner.instance) {
            co_await Whisper_runInstance(io, std::move(runner.instance));
        }
    }
}

xec::coro<void> Whisper_runSession(StreamEndpoint ep) {
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
        auto ex = co_await xec::executor{};
        IoEndpoint io(std::move(ep), ex);

        co_await io.push(Frame_stateChange(Schema::id));

        Runner runner;

        while (true)
        {
            auto f = co_await io.poll();
            co_await io.push(runner.dispatch(*f));
            if (runner.model) {
                co_await Whisper_runModel(io, std::move(runner.model));
            }
        }
    }
    catch (io::stream_closed_error&) {
        co_return;
    }
}

ServiceInfo g_serviceInfo = {
    .name = "ac whisper.cpp",
    .vendor = "Alpaca Core",
};

struct WhisperService final : public Service {
    WhisperService(BackendWorkerStrand& ws) : m_workerStrand(ws) {}

    BackendWorkerStrand& m_workerStrand;

    virtual const ServiceInfo& info() const noexcept override {
        return g_serviceInfo;
    }

    virtual void createSession(frameio::StreamEndpoint ep, Dict) override {
        co_spawn(m_workerStrand.executor(), Whisper_runSession(std::move(ep)));
    }
};

struct WhisperServiceFactory final : public ServiceFactory {
    virtual const ServiceInfo& info() const noexcept override {
        return g_serviceInfo;
    }
    virtual std::unique_ptr<Service> createService(Backend& backend) const override {
        auto svc = std::make_unique<WhisperService>(backend.gpuWorkerStrand());
        return svc;
    }
};

}

} // namespace ac::local

namespace ac::whisper {

void init() {
    initLibrary();
}

std::vector<const local::ServiceFactory*> getFactories() {
    static local::WhisperServiceFactory factory;
    return {&factory};
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
        .getServiceFactories = getFactories,
    };
}

} // namespace ac::whisper

