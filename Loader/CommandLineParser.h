#ifndef __COMMANDLINEPARSER_H_INCLIDED_
#define __COMMANDLINEPARSER_H_INCLIDED_

const unsigned
    OPTION_MASK_HELP = 0x00000001,
    OPTION_MASK_INFO = 0x00000002,
    OPTION_MASK_PROGRAM = 0x00000004,
    OPTION_MASK_VERIFY = 0x00000008,
    OPTION_MASK_LOAD = 0x00000010,
    OPTION_MASK_ERASE = 0x00000020,
    OPTION_MASK_TIMEOUT = 0x00000040,
    OPTION_MASK_NO_RUN = 0x00000080,
    OPTION_MASK_FORCE = 0x00000100,
    OPTION_MASK_MODEL = 0x00000200,
    OPTION_MASK_ALL = 0x00000400,
    OPTION_MASK_NO_SMART = 0x00000800;
    
struct CommandLineParams
{
    std::vector<std::string> args;
    unsigned optionMask = 0;
    std::string model;
    unsigned timeout = 0;
};

void commandLineParser(int argc, char *argv[], CommandLineParams *params);

#endif // !__COMMANDLINEPARSER_H_INCLIDED_
