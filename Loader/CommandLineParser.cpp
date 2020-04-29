#include "Stable.h"
#include "CommandLineParser.h"
#include "ErrorExit.h"

struct OptionInfo
{
    unsigned mask;
    const char *sortName;
    const char *longName;
};

static const OptionInfo OPTION_INFO[] = {
    { OPTION_MASK_HELP, "h", "help" },
    { OPTION_MASK_INFO, "i", "info" },
    { OPTION_MASK_PROGRAM, "p", "program" },
    { OPTION_MASK_VERIFY, "v", "verify" },
    { OPTION_MASK_LOAD, "l", "load" },
    { OPTION_MASK_ERASE, "e", "erase" },
    { OPTION_MASK_TIMEOUT, "t", "timeout" },
    { OPTION_MASK_NO_RUN, "r", "no-run" },
    { OPTION_MASK_FORCE, "f", "force" },
    { OPTION_MASK_MODEL, "m", "model" },
    { OPTION_MASK_ALL, "a", "all" },
    { OPTION_MASK_NO_SMART, "s", "no-smart" },
};

static size_t getOptionIndex(const char *optionName, const char *originalParam)
{
    for (size_t index = 0; index < sizeof OPTION_INFO / sizeof OPTION_INFO[0]; ++index)
    {
        if ((stricmp(optionName, OPTION_INFO[index].sortName) == 0)
            || (stricmp(optionName, OPTION_INFO[index].longName) == 0))
        {
            return index;
        }
    }

    errorExit("Unknown option: %s", originalParam);

    return 0;
}

// the first char of param is '/' of '-'
static void parseOption(const char *param, std::string *optionName, std::string *optionValue)
{
    const char *nameStart = param;
    ++nameStart;
    if (*nameStart == '-') ++nameStart;

    const char *nameEnd = nameStart;
    while ((*nameEnd != 0x00) && (*nameEnd != '=')) ++nameEnd;
    
    const char *valueStart = nameEnd;
    if (*valueStart == '=') ++valueStart;

    *optionName = std::string(nameStart, nameEnd);
    if (optionName->empty()) errorExit("Wrong option: %s", param);

    *optionValue = std::string(valueStart);
}

static unsigned parseUnsigned(const char *str, const char *originalParam)
{
    unsigned result = 0;

    do
    {
        char chr = *(str++);
        if ((chr < '0') || (chr > '9')) errorExit("Number parse error: %s", originalParam);
        unsigned newValue = result * 10 + chr - '0';
        if (newValue < result) errorExit("Number parse error: %s", originalParam);
        result = newValue;
    }
    while (*str != 0x00);

    return result;
}

void commandLineParser(int argc, char *argv[], CommandLineParams *params)
{
    *params = CommandLineParams();

    for (int i = 1; i < argc; ++i)
    {
        const char *param = argv[i];
        if ((param[0] == '-') || (param[0] == '/'))
        {
            std::string optionName;
            std::string optionValue;
            parseOption(param, &optionName, &optionValue);
            size_t index = getOptionIndex(optionName.c_str(), param);

            unsigned optionMask = OPTION_INFO[index].mask;
            if ((optionMask & params->optionMask) != 0)
            {
                errorExit("Dupliacted option: %s", param);
            }
            params->optionMask |= optionMask;

            if (optionMask == OPTION_MASK_TIMEOUT)
            {
                params->timeout = parseUnsigned(optionValue.c_str(), param);
            }
            else if (optionMask == OPTION_MASK_MODEL)
            {
                if (optionValue.empty()) errorExit("Model name must be defined: %s", param);
                params->model = optionValue;
            }
            else if (!optionValue.empty())
            {
                errorExit("Option syntax error: %s", param);
            }
        }
        else
        {
            params->args.push_back(param);
        }
    }
}
