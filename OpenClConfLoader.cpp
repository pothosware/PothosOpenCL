// Copyright (c) 2020 Nicholas Corgan
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Callable.hpp>
#include <Pothos/Exception.hpp>
#include <Pothos/Framework.hpp>
#include <Pothos/Object.hpp>
#include <Pothos/Plugin.hpp>
#include <Pothos/Proxy.hpp>
#include <Pothos/Util/BlockDescription.hpp>

#include <Poco/File.h>
#include <Poco/Path.h>
#include <Poco/StringTokenizer.h>

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <vector>

struct OpenClBlockArgs
{
    std::string source;
    std::string kernelName;

    std::vector<std::string> inputTypes;
    std::vector<std::string> outputTypes;
};

static Pothos::Proxy openClBlockFactory(
    const std::string& device,
    const OpenClBlockArgs& blockArgs)
{
    auto openClBlock = Pothos::BlockRegistry::make(
                           "/blocks/opencl_kernel",
                           device,
                           blockArgs.inputTypes,
                           blockArgs.outputTypes);
    openClBlock.call(
        "setSource",
        blockArgs.source,
        blockArgs.kernelName);

    return openClBlock;
}

static inline std::vector<std::string> stringTokenizerToVector(const Poco::StringTokenizer& tokenizer)
{
    std::vector<std::string> stdVector;
    std::transform(
        tokenizer.begin(),
        tokenizer.end(),
        std::back_inserter(stdVector),
        [](const std::string& str){return str;});

    return stdVector;
}

static std::vector<Pothos::PluginPath> OpenClConfLoader(const std::map<std::string, std::string>& config)
{
    static const auto tokOptions = Poco::StringTokenizer::TOK_IGNORE_EMPTY | Poco::StringTokenizer::TOK_TRIM;
    static const std::string tokSep(" \t");
    
    // Set by calling function
    const auto confFilePathIter = config.find("confFilePath");
    if(confFilePathIter == config.end()) throw Pothos::Exception("No conf filepath");
    const auto rootDir = Poco::Path(confFilePathIter->second).makeParent();

    //
    // Source must be a path
    //
    std::string absSource;
    auto sourceIter = config.find("source");
    if(sourceIter == config.end()) throw Pothos::Exception("No source");
    absSource = Poco::Path(rootDir, sourceIter->second).toString();
    if(!Poco::File(absSource).exists())
    {
        throw Pothos::FileNotFoundException(absSource);
    }

    auto kernelNameIter = config.find("kernel_name");
    if(kernelNameIter == config.end()) throw Pothos::Exception("No kernel name");

    auto inputTypesIter = config.find("input_types");
    if(inputTypesIter == config.end()) throw Pothos::Exception("No input types");
    auto inputTypes = stringTokenizerToVector(Poco::StringTokenizer(inputTypesIter->second, tokSep, tokOptions));

    auto outputTypesIter = config.find("output_types");
    if(outputTypesIter == config.end()) throw Pothos::Exception("No output types");
    auto outputTypes = stringTokenizerToVector(Poco::StringTokenizer(outputTypesIter->second, tokSep, tokOptions));

    //
    // Generate and store JSON block docs
    //
    Pothos::Util::BlockDescriptionParser parser;
    parser.feedFilePath(absSource);

    const auto& factories = parser.listFactories();
    assert(factories.size() == 1);

    Pothos::PluginPath pluginPath = Pothos::PluginPath("/blocks/docs", pluginPath);
    Pothos::PluginRegistry::add(pluginPath, parser.getJSONObject(factories[0]));

    //
    // Register all factory paths, using the parameters from the config file.
    //
    OpenClBlockArgs blockArgs =
    {
        absSource,
        kernelNameIter->second,
        inputTypes,
        outputTypes
    };

    auto blockFactory = Pothos::Callable(&openClBlockFactory).bind(blockArgs, 2);
    Pothos::PluginRegistry::addCall(pluginPath, blockFactory);

    return std::vector<Pothos::PluginPath>{pluginPath};
}

//
// Register conf loader
//
pothos_static_block(pothosRegisterOpenClConfLoader)
{
    Pothos::PluginRegistry::addCall(
        "/framework/conf_loader/opencl",
        &OpenClConfLoader);
}
