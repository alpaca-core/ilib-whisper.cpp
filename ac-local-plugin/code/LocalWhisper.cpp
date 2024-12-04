// Copyright (c) Alpaca Core
// SPDX-License-Identifier: MIT
//
#include <ac/whisper/Instance.hpp>
#include <ac/whisper/Init.hpp>
#include <ac/whisper/Model.hpp>

#include <ac/local/Instance.hpp>
#include <ac/local/Model.hpp>
#include <ac/local/ModelLoader.hpp>

#include <ac/schema/WhisperCpp.hpp>
#include <ac/schema/DispatchHelpers.hpp>

#include <astl/move.hpp>
#include <astl/move_capture.hpp>
#include <astl/iile.h>
#include <astl/throw_stdex.hpp>
#include <astl/workarounds.h>

#include "aclp-whisper-version.h"
#include "aclp-whisper-interface.hpp"

namespace ac::local {

namespace {

class WhisperInstance final : public Instance {
    std::shared_ptr<whisper::Model> m_model;
    whisper::Instance m_instance;
    schema::OpDispatcherData m_dispatcherData;
public:
    using Schema = ac::local::schema::WhisperCppLoader::InstanceGeneral;
    using Interface = ac::local::schema::WhisperCppInterface;

    WhisperInstance(std::shared_ptr<whisper::Model> model)
        : m_model(astl::move(model))
        , m_instance(*m_model, {})
    {
        schema::registerHandlers<Interface::Ops>(m_dispatcherData, *this);
    }

    Interface::OpTranscribe::Return on(Interface::OpTranscribe, Interface::OpTranscribe::Params params) {
        const auto& blob = params.audio.value();

        auto pcmf32 = reinterpret_cast<const float*>(blob.data());
        auto pcmf32Size = blob.size() / sizeof(float);

        return {
            .result = m_instance.transcribe(std::span{pcmf32, pcmf32Size})
        };
    }

    virtual Dict runOp(std::string_view op, Dict params, ProgressCb) override {
        auto ret = m_dispatcherData.dispatch(op, astl::move(params));
        if (!ret) {
            throw_ex{} << "whisper: unknown op: " << op;
        }
        return *ret;
    }
};

class WhisperModel final : public Model {
    std::shared_ptr<whisper::Model> m_model;
public:
    using Schema = ac::local::schema::WhisperCppLoader;

    WhisperModel(const std::string& gguf, whisper::Model::Params params)
        : m_model(std::make_shared<whisper::Model>(gguf.c_str(), astl::move(params)))
    {}

    virtual std::unique_ptr<Instance> createInstance(std::string_view type, Dict) override {
        if (type == "general") {
            return std::make_unique<WhisperInstance>(m_model);
        }
        else {
            throw_ex{} << "whisper: unknown instance type: " << type;
            MSVC_WO_10766806();
        }
    }
};

class WhisperModelLoader final : public ModelLoader {
public:
    virtual const Info& info() const noexcept override {
        static Info i = {
            .name = "ac whisper.cpp",
            .vendor = "Alpaca Core",
        };
        return i;
    }

    virtual bool canLoadModel(const ModelAssetDesc& desc, const Dict&) const noexcept override {
        return desc.type == "whisper.cpp bin";
    }

    virtual ModelPtr loadModel(ModelAssetDesc desc, Dict /*params*/, ProgressCb /*progressCb*/) override {
        if (desc.assets.size() != 1) throw_ex{} << "whisper: expected exactly one local asset";
        auto& bin = desc.assets.front().path;
        whisper::Model::Params modelParams;
        return std::make_shared<WhisperModel>(bin, modelParams);
    }
};
}

} // namespace ac::local

namespace ac::whisper {

void init() {
    initLibrary();
}

std::vector<ac::local::ModelLoaderPtr> getLoaders() {
    std::vector<ac::local::ModelLoaderPtr> ret;
    ret.push_back(std::make_unique<local::WhisperModelLoader>());
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
        .getLoaders = getLoaders,
    };
}

} // namespace ac::whisper

