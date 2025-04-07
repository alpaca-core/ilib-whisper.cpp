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
#include <ac/schema/FrameHelpers.hpp>
#include <ac/schema/StateChange.hpp>
#include <ac/schema/Error.hpp>
#include <ac/schema/OpTraits.hpp>

#include <ac/frameio/IoEndpoint.hpp>

#include <ac/xec/coro.hpp>
#include <ac/xec/co_spawn.hpp>
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

namespace {

struct LocalWhisper {
    Backend& m_backend;
public:
    LocalWhisper(Backend& backend)
        : m_backend(backend)
    {}

    static Frame unknownOpError(const Frame& f) {
        return Frame_from(schema::Error{}, "whisper: unknown op: " + f.op);
    }

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

    xec::coro<void> runInstance(IoEndpoint& io, whisper::Instance& instance) {
        using Schema = sc::StateInstance;
        co_await io.push(Frame_from(schema::StateChange{}, Schema::id));

        while(true) {
            auto f = co_await io.poll();
            Frame err;

            try {
                if (auto iparams = Frame_optTo(schema::OpParams<Schema::OpTranscribe>{}, *f)) {
                    const auto& pcmf32 = iparams->audio.value();

                    co_await io.push(Frame_from(Schema::OpTranscribe{}, {
                        .text = instance.transcribe(pcmf32)
                    }));
                } else {
                    err = unknownOpError(*f);
                }
            }
            catch (std::runtime_error& e) {
                err = Frame_from(schema::Error{}, e.what());
            }

            if (!err.op.empty()) {
                co_await io.push(err);
            }
        }
    }

    xec::coro<void> runModel(IoEndpoint& io, sc::StateWhisper::OpLoadModel::Params& params) {
        auto modelPath = params.binPath.valueOr("");

        whisper::Model::Params wParams;
        wParams.gpu = params.useGpu.valueOr(true);

        whisper::Model model(modelPath.c_str(), std::move(wParams));

        using Schema = sc::StateModelLoaded;
        co_await io.push(Frame_from(schema::StateChange{}, Schema::id));

        while (true) {
            auto f = co_await io.poll();
            Frame err;
            try {
                if (auto iparams = Frame_optTo(schema::OpParams<Schema::OpStartInstance>{}, *f)) {
                    whisper::Instance::InitParams wiparams = InstanceParams_fromSchema(*iparams);
                    whisper::Instance instance(model, std::move(wiparams));
                    co_await runInstance(io, instance);
                }
                else {
                    err = unknownOpError(*f);
                }
            }
            catch (std::runtime_error& e) {
                 err = Frame_from(schema::Error{}, e.what());
            }

            co_await io.push(err);
        }
    }

    xec::coro<void> runSession(IoEndpoint& io) {
        using Schema = sc::StateWhisper;

        co_await io.push(Frame_from(schema::StateChange{}, Schema::id));

        while (true) {
            auto f = co_await io.poll();

            Frame err;

            try {
                if (auto lm = Frame_optTo(schema::OpParams<Schema::OpLoadModel>{}, * f)) {
                    co_await runModel(io, *lm);
                }
                else {
                    err = unknownOpError(*f);
                }
            }
            catch (std::runtime_error& e) {
                err = Frame_from(schema::Error{}, e.what());
            }

            co_await io.push(err);
        }
    }

    xec::coro<void> run(frameio::StreamEndpoint ep, xec::strand ex) {
        try {
            IoEndpoint io(std::move(ep), ex);
            co_await runSession(io);
        }
        catch (io::stream_closed_error&) {
            co_return;
        }
    }
};

ServiceInfo g_serviceInfo = {
    .name = "ac whisper.cpp",
    .vendor = "Alpaca Core",
};

struct WhisperService final : public Service {
    WhisperService(BackendWorkerStrand& ws) : m_workerStrand(ws) {}

    BackendWorkerStrand& m_workerStrand;
    std::shared_ptr<LocalWhisper> whisper;

    virtual const ServiceInfo& info() const noexcept override {
        return g_serviceInfo;
    }

    virtual void createSession(frameio::StreamEndpoint ep, Dict) override {
        whisper = std::make_shared<LocalWhisper>(m_workerStrand.backend);
        co_spawn(m_workerStrand.executor(), whisper->run(std::move(ep), m_workerStrand.executor()));
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

