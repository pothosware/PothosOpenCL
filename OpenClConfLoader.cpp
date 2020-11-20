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
#include <Poco/Optional.h>
#include <Poco/Path.h>
#include <Poco/StringTokenizer.h>

#include <algorithm>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

struct FactoryArgs
{
    // Required
    std::string source;
    std::string kernelName;
    std::vector<std::string> inputTypes;
    std::vector<std::string> outputTypes;

    // Optional, will be set if given
    Poco::Optional<size_t> optLocalSize;
    Poco::Optional<size_t> optGlobalFactor;
    Poco::Optional<size_t> optProductionFactor;
};

struct BlockDescriptionArgs
{
    // Required
    std::string blockName;
    std::vector<std::string> categories;

    // Optional, will be set if given
    Poco::Optional<std::string> optDescription;
    Poco::Optional<std::vector<std::string>> optKeywords;
};

static Pothos::Object opaqueOpenClBlockFactory(
    const std::string& factory,
    const FactoryArgs& factoryArgs,
    const Pothos::Object* args,
    const size_t numArgs)
{
    auto openClBlockPlugin = Pothos::PluginRegistry::get("/blocks/blocks/opencl_kernel");

    // The OpenCL kernel block takes in the input and output types, which are provided
    // by the configuration file, so append those onto the end and pass in the new
    // list.
    std::vector<Pothos::Object> argsVector(args, args+numArgs);
    argsVector.emplace_back(factoryArgs.inputTypes);
    argsVector.emplace_back(factoryArgs.outputTypes);

    auto callable = openClBlockPlugin.getObject().extract<Pothos::Callable>();
    auto openClBlock = callable.opaqueCall(argsVector.data(), argsVector.size());

    openClBlock.ref<Pothos::Block*>()->setName(factory);
    openClBlock.ref<Pothos::Block*>()->call(
        "setSource",
        factoryArgs.kernelName,
        factoryArgs.source);

    if(factoryArgs.optLocalSize.isSpecified())
    {
        openClBlock.ref<Pothos::Block*>()->call(
            "setLocalSize",
            factoryArgs.optLocalSize.value());
    }
    if(factoryArgs.optGlobalFactor.isSpecified())
    {
        openClBlock.ref<Pothos::Block*>()->call(
            "setGlobalFactor",
            factoryArgs.optGlobalFactor.value());
    }
    if(factoryArgs.optProductionFactor.isSpecified())
    {
        openClBlock.ref<Pothos::Block*>()->call(
            "setProductionFactor",
            factoryArgs.optProductionFactor.value());
    }

    return openClBlock;
}

//
// Generate block description
//

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

static void generateLocalSizeStrings(
    const Poco::Optional<size_t>& optLocalSize,
    std::string* descriptionOut,
    std::string* setterOut)
{
    static const std::string DESCRIPTION =
        " * |param localSize[Local Size] The number of work units/resources to allocate. \n"
        " * This controls the parallelism of the kernel execution. \n"
        " * |default 2 \n";

    static const std::string SETTER =
        " * |setter setLocalSize(localSize) \n";

    (*descriptionOut) = optLocalSize.isSpecified() ? "" : DESCRIPTION;
    (*setterOut) = optLocalSize.isSpecified() ? "" : SETTER;
}

static void generateGlobalFactorStrings(
    const Poco::Optional<size_t>& optGlobalFactor,
    std::string* descriptionOut,
    std::string* setterOut)
{
    static const std::string DESCRIPTION =
        " * |param globalFactor[Global Factor] This factor controls the global size. \n"
        " * The global size is the number of kernel iterarions per call. \n"
        " * Global size = number of input elements * global factor. \n"
        " * |default 1.0 \n";

    static const std::string SETTER =
        " * |setter setGlobalFactor(globalFactor) \n";

    (*descriptionOut) = optGlobalFactor.isSpecified() ? "" : DESCRIPTION;
    (*setterOut) = optGlobalFactor.isSpecified() ? "" : SETTER;
}

static void generateProductionFactorStrings(
    const Poco::Optional<size_t>& optProductionFactor,
    std::string* descriptionOut,
    std::string* setterOut)
{
    static const std::string DESCRIPTION =
        " * |param productionFactor[Production Factor] This factor controls the elements produced. \n"
        " * For each call to work, elements produced = number of input elements * production factor. \n"
        " * |default 1.0 \n";

    static const std::string SETTER =
        " * |setter setProductionFactor(productionFactor)\n";

    (*descriptionOut) = optProductionFactor.isSpecified() ? "" : DESCRIPTION;
    (*setterOut) = optProductionFactor.isSpecified() ? "" : SETTER;
}

// Since Poco::format can only take in so many parameters
static void generateParamsAndSetters(
    const FactoryArgs& factoryArgs,
    std::string* paramsOut,
    std::string* settersOut)
{
    std::string localSizeParam;
    std::string globalFactorParam;
    std::string productionFactorParam;

    std::string localSizeSetter;
    std::string globalFactorSetter;
    std::string productionFactorSetter;

    generateLocalSizeStrings(
        factoryArgs.optLocalSize,
        &localSizeParam,
        &localSizeSetter);
    generateGlobalFactorStrings(
        factoryArgs.optGlobalFactor,
        &globalFactorParam,
        &globalFactorSetter);
    generateProductionFactorStrings(
        factoryArgs.optProductionFactor,
        &productionFactorParam,
        &productionFactorSetter);

    (*paramsOut) = Poco::format(
                       "%s%s%s",
                       localSizeParam,
                       globalFactorParam,
                       productionFactorParam);
    (*settersOut) = Poco::format(
                        "%s%s%s",
                        localSizeSetter,
                        globalFactorSetter,
                        productionFactorSetter);
}

static std::string generateBlockDescription(
    const FactoryArgs& factoryArgs,
    const BlockDescriptionArgs& blockDescriptionArgs,
    const std::string& factory)
{
    static const std::string FORMAT =
        "/***********************************************************************\n"
        " * |PothosDoc %s \n"
        " * %s\n"
        "%s\n"
        "%s\n"
        " *\n"
        " * |param deviceId[Device ID] A markup to specify OpenCL platform and device. \n"
        " * The markup takes the format [platform index]:[device index] \n"
        " * The platform index represents a platform ID found in clGetPlatformIDs(). \n"
        " * The device index represents a device ID found in clGetDeviceIDs(). \n"
        " * |default \"0:0\" \n"
        " *\n"
        "%s\n"
        " * |factory %s(deviceId) \n"
        "%s\n"
        " **********************************************************************/";

    std::string categoryString;
    for(const auto& category: blockDescriptionArgs.categories)
    {
        categoryString += Poco::format(" * |category %s\n", category);
    }

    std::string keywordString;
    if(blockDescriptionArgs.optKeywords.isSpecified())
    {
        for(const auto& keyword: blockDescriptionArgs.optKeywords.value())
        {
            keywordString += Poco::format(" * |keyword %s\n", keyword);
        }
    }

    std::string paramString;
    std::string setterString;
    generateParamsAndSetters(
        factoryArgs,
        &paramString,
        &setterString);

    return Poco::format(
               FORMAT,
               blockDescriptionArgs.blockName,
               blockDescriptionArgs.optDescription.value(""),
               categoryString,
               keywordString,
               paramString,
               factory,
               setterString);
}

template <typename T>
static T convertString(const std::string& str)
{
    T ret;

    std::stringstream sstream(str);
    sstream >> ret;

    return ret;
}

//
// Register code
//

static std::vector<Pothos::PluginPath> OpenClConfLoader(const std::map<std::string, std::string>& config)
{
    static const auto tokOptions = Poco::StringTokenizer::TOK_IGNORE_EMPTY | Poco::StringTokenizer::TOK_TRIM;
    static const std::string tokSep(" \t");

    FactoryArgs factoryArgs;
    BlockDescriptionArgs blockDescriptionArgs;

    // Set by calling function
    const auto confFilePathIter = config.find("confFilePath");
    if(confFilePathIter == config.end()) throw Pothos::Exception("No conf filepath");
    const auto rootDir = Poco::Path(confFilePathIter->second).makeParent();

    //
    // Factory parameters
    //

    // Policy: source must be a path
    auto sourceIter = config.find("source");
    if(sourceIter == config.end()) throw Pothos::Exception("No source");
    factoryArgs.source = Poco::Path(rootDir, sourceIter->second).toString();
    if(!Poco::File(factoryArgs.source).exists())
    {
        throw Pothos::FileNotFoundException(factoryArgs.source);
    }

    auto kernelNameIter = config.find("kernel_name");
    if(kernelNameIter != config.end()) factoryArgs.kernelName = kernelNameIter->second;
    else throw Pothos::Exception("No kernel name");

    auto inputTypesIter = config.find("input_types");
    if(inputTypesIter == config.end()) throw Pothos::Exception("No input types");
    factoryArgs.inputTypes = stringTokenizerToVector(Poco::StringTokenizer(inputTypesIter->second, tokSep, tokOptions));

    auto outputTypesIter = config.find("output_types");
    if(outputTypesIter == config.end()) throw Pothos::Exception("No output types");
    factoryArgs.outputTypes = stringTokenizerToVector(Poco::StringTokenizer(outputTypesIter->second, tokSep, tokOptions));

    auto localSizeIter = config.find("local_size");
    if(localSizeIter != config.end())
    {
        factoryArgs.optLocalSize = convertString<size_t>(localSizeIter->second);
    }

    auto globalFactorIter = config.find("global_factor");
    if(globalFactorIter != config.end())
    {
        factoryArgs.optGlobalFactor = convertString<size_t>(globalFactorIter->second);
    }

    auto productionFactorIter = config.find("production_factor");
    if(productionFactorIter != config.end())
    {
        factoryArgs.optProductionFactor = convertString<size_t>(productionFactorIter->second);
    }

    //
    // BlockDescription values
    //

    auto blockNameIter = config.find("block_name");
    blockDescriptionArgs.blockName = (blockNameIter != config.end()) ? blockNameIter->second : factoryArgs.kernelName;

    auto categoriesIter = config.find("categories");
    if(categoriesIter != config.end())
    {
        blockDescriptionArgs.categories = stringTokenizerToVector({outputTypesIter->second, tokSep, tokOptions});
    }
    else blockDescriptionArgs.categories = {Poco::Path(factoryArgs.source).getBaseName()};

    auto keywordsIter = config.find("keywords");
    if(keywordsIter != config.end())
    {
        blockDescriptionArgs.optKeywords = stringTokenizerToVector({outputTypesIter->second, tokSep, tokOptions});
    }

    auto descriptionIter = config.find("description");
    if(descriptionIter != config.end()) blockDescriptionArgs.optDescription = descriptionIter->second;

    std::string factory;
    auto factoryIter = config.find("factory");
    if(factoryIter != config.end()) factory = factoryIter->second;
    else throw Pothos::Exception("No factory");

    //
    // Generate and store JSON block docs
    //
    Pothos::Util::BlockDescriptionParser parser;
    auto blockDescription = generateBlockDescription(
                                factoryArgs,
                                blockDescriptionArgs,
                                factory);
    std::stringstream stream;
    stream << blockDescription;
    parser.feedStream(stream);

    //
    // Register all factory paths, using the parameters from the config file.
    //
    auto blockFactory = Pothos::Callable(&opaqueOpenClBlockFactory)
                            .bind(factory, 0)
                            .bind(factoryArgs, 1);
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
