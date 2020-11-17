// Copyright (c) 2020 Nicholas Corgan
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Exception.hpp>
#include <Pothos/Framework.hpp>
#include <Pothos/Object.hpp>
#include <Pothos/Plugin.hpp>
#include <Pothos/Proxy.hpp>
#include <Pothos/Util/BlockDescription.hpp>

#include <Poco/File.h>
#include <Poco/Path.h>
#include <Poco/StringTokenizer.h>

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

static std::vector<Pothos::PluginPath> OpenClConfLoader(const std::map<std::string, std::string>& config)
{
    std::vector<Pothos::PluginPath> entries;
    static const auto tokOptions = Poco::StringTokenizer::TOK_IGNORE_EMPTY | Poco::StringTokenizer::TOK_TRIM;
    static const std::string tokSep(" \t");
    
    // Set by caller
    const auto confFilePathIter = config.find("confFilePath");
    if(confFilePathIter == config.end()) throw Pothos::Exception("No conf filepath");
    const auto rootDir = Poco::Path(confFilePathIter->second).makeParent();

    // TODO: sources
    auto sourceIter = config.find("source");
    if(sourceIter == config.end()) throw Pothos::Exception("No source");

    auto kernelNameIter = config.find("kernel_name");
    if(kernelNameIter == config.end()) throw Pothos::Exception("No kernel name");

    std::vector<Pothos::PluginPath> factories;
    std::vector<std::string> docSources;

    auto docSourcesIter = config.find("doc_sources");
    if(docSourcesIter != config.end())
    {
        for(const auto& docSource: Poco::StringTokenizer(docSourcesIter->second, tokSep, tokOptions))
        {
            const auto absPath = Poco::Path(docSource).makeAbsolute(rootDir);
            docSources.push_back(absPath.toString());
        }
    }
    else
    {
        const auto absPath = Poco::Path(sourceIter->second).makeAbsolute(rootDir);
        docSources.push_back(absPath.toString());
    }

    // TODO: test compiling source outside block

    //
    // Generate and store JSON block docs
    //
    Pothos::Util::BlockDescriptionParser parser;
    for(const auto& source: docSources) parser.feedFilePath(source);
    for(const auto& factory: parser.listFactories())
    {
        const auto pluginPath = Pothos::PluginPath("/blocks/docs", factory);
        Pothos::PluginRegistry::add(pluginPath, parser.getJSONObject(factory));

        factories.emplace_back("/blocks", factory);
    }

    return entries;
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
