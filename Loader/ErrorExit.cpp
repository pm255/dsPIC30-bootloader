#include "Stable.h"
#include "ErrorExit.h"

void errorExit(const char *message, ...)
{
    char buffer[1024];

    va_list args;
    va_start(args, message);
    vsprintf_s(buffer, message, args);
    va_end(args);

    printf("\n%s\n", buffer);

    exit(1);
}
