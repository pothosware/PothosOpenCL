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
#include <Poco/Format.h>
#include <Poco/Path.h>
#include <Poco/StringTokenizer.h>

#include <algorithm>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

struct OpenClBlockArgs
{
    std::string source;
    std::string kernelName;

    std::vector<std::string> inputTypes;
    std::vector<std::string> outputTypes;
};

static Pothos::Object opaqueOpenClBlockFactory(
    const std::string& factory,
    const OpenClBlockArgs& blockArgs,
    const Pothos::Object* args,
    const size_t numArgs)
{
    auto openClBlockPlugin = Pothos::PluginRegistry::get("/blocks/blocks/opencl_kernel");

    // The OpenCL kernel block takes in the input and output types, which are provided
    // by the configuration file, so append those onto the end and pass in the new
    // list.
    std::vector<Pothos::Object> argsVector(args, args+numArgs);
    argsVector.emplace_back(blockArgs.inputTypes);
    argsVector.emplace_back(blockArgs.outputTypes);

    auto callable = openClBlockPlugin.getObject().extract<Pothos::Callable>();
    auto openClBlock = callable.opaqueCall(argsVector.data(), argsVector.size());

    openClBlock.ref<Pothos::Block*>()->setName(factory);
    openClBlock.ref<Pothos::Block*>()->call(
        "setSource",
        blockArgs.kernelName,
        blockArgs.source);

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

static std::string generateBlockDescription(
    const std::string& blockName,
    const std::vector<std::string>& categories,
    const std::vector<std::string>& keywords,
    const std::string& description,
    const std::string& factory)
{
    static const std::string FORMAT =
        "/***********************************************************************\n"
        " * |PothosDoc %s \n"
        " * %s\n"
        "%s"
        "%s"
        " *\n"
        " * |param deviceId[Device ID] A markup to specify OpenCL platform and device. \n"
        " * The markup takes the format [platform index]:[device index] \n"
        " * The platform index represents a platform ID found in clGetPlatformIDs(). \n"
        " * The device index represents a device ID found in clGetDeviceIDs(). \n"
        " * |default \"0:0\" \n"
        " *\n"
        " * |param localSize[Local Size] The number of work units/resources to allocate. \n"
        " * This controls the parallelism of the kernel execution. \n"
        " * |default 2 \n"
        " *\n"
        " * |param globalFactor[Global Factor] This factor controls the global size. \n"
        " * The global size is the number of kernel iterarions per call. \n"
        " * Global size = number of input elements * global factor. \n"
        " * |default 1.0 \n"
        " *\n"
        " * |param productionFactor[Production Factor] This factor controls the elements produced. \n"
        " * For each call to work, elements produced = number of input elements * production factor. \n"
        " * |default 1.0 \n"
        " *\n"
        " * |factory %s(deviceId) \n"
        " * |setter setLocalSize(localSize) \n"
        " * |setter setGlobalFactor(globalFactor) \n"
        " * |setter setProductionFactor(productionFactor)\n"
        " **********************************************************************/";

    std::string categoryString;
    for(const auto& category: categories)
    {
        categoryString += Poco::format(" * |category %s\n", category);
    }

    std::string keywordString;
    for(const auto& keyword: keywords)
    {
        keywordString += Poco::format(" * |keyword %s\n", keyword);
    }

    return Poco::format(FORMAT, blockName, description, categoryString, keywordString, factory);
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

    std::string kernelName;
    auto kernelNameIter = config.find("kernel_name");
    if(kernelNameIter != config.end()) kernelName = kernelNameIter->second;
    else throw Pothos::Exception("No kernel name");

    auto inputTypesIter = config.find("input_types");
    if(inputTypesIter == config.end()) throw Pothos::Exception("No input types");
    auto inputTypes = stringTokenizerToVector(Poco::StringTokenizer(inputTypesIter->second, tokSep, tokOptions));

    auto outputTypesIter = config.find("output_types");
    if(outputTypesIter == config.end()) throw Pothos::Exception("No output types");
    auto outputTypes = stringTokenizerToVector(Poco::StringTokenizer(outputTypesIter->second, tokSep, tokOptions));

    std::string blockName;
    auto blockNameIter = config.find("block_name");
    blockName = (blockNameIter != config.end()) ? blockNameIter->second : kernelName;

    std::vector<std::string> categories;
    auto categoriesIter = config.find("categories");
    if(categoriesIter != config.end())
    {
        categories = stringTokenizerToVector(Poco::StringTokenizer(outputTypesIter->second, tokSep, tokOptions));
    }
    else categories = {Poco::Path(absSource).getBaseName()};

    std::vector<std::string> keywords;
    auto keywordsIter = config.find("keywords");
    if(keywordsIter != config.end())
    {
        keywords = stringTokenizerToVector(Poco::StringTokenizer(outputTypesIter->second, tokSep, tokOptions));
    }

    std::string description;
    auto descriptionIter = config.find("description");
    if(descriptionIter != config.end()) description = descriptionIter->second;

    std::string factory;
    auto factoryIter = config.find("factory");
    if(factoryIter != config.end()) factory = factoryIter->second;
    else throw Pothos::Exception("No factory");

    //
    // Generate and store JSON block docs
    //
    Pothos::Util::BlockDescriptionParser parser;
    auto blockDescription = generateBlockDescription(
                                blockName,
                                categories,
                                keywords,
                                description,
                                factory);
    std::stringstream stream;
    stream << blockDescription;
    parser.feedStream(stream);

    //
    // Register all factory paths, using the parameters from the config file.
    //
    OpenClBlockArgs blockArgs =
    {
        absSource,
        kernelName,
        inputTypes,
        outputTypes
    };

    auto blockFactory = Pothos::Callable(&opaqueOpenClBlockFactory)
                            .bind(factory, 0)
                            .bind(blockArgs, 1);
    Pothos::PluginRegistry::addCall("/blocks"+factory, blockFactory);
    Pothos::PluginRegistry::add("/blocks/docs"+factory, parser.getJSONObject(factory));

    return
    {
        "/blocks"+factory,
        "/blocks/docs"+factory
    };
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
