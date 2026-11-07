#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include "opener_error.h"
#include "opener_api.h"

const int kErrorMessageBufferSize = 255;

int GetSocketErrorNumber(void)
{
    return errno;
}

char *GetErrorMessage(int error_number)
{
    const char *error_name = "UNKNOWN";

    switch(error_number)
    {
        case 4:   error_name = "EINTR"; break;
        case 5:   error_name = "EIO"; break;
        case 6:   error_name = "ENXIO"; break;
        case 9:   error_name = "EBADF"; break;
        case 11:  error_name = "EAGAIN/EWOULDBLOCK"; break;
        case 14:  error_name = "EFAULT"; break;
        case 16:  error_name = "EBUSY"; break;
        case 23:  error_name = "ENFILE"; break;
        case 90:  error_name = "EMSGSIZE"; break;
        case 92:  error_name = "ENOPROTOOPT"; break;
        case 95:  error_name = "EOPNOTSUPP"; break;
        case 98:  error_name = "EADDRINUSE"; break;
        case 103: error_name = "ECONNABORTED"; break;
        case 104: error_name = "ECONNRESET"; break;
        case 105: error_name = "ENOBUFS"; break;
        case 106: error_name = "EISCONN"; break;
        case 107: error_name = "ENOTCONN"; break;
        case 113: error_name = "EHOSTUNREACH"; break;
        case 114: error_name = "EALREADY"; break;
        case 115: error_name = "EINPROGRESS"; break;
    }

    char *error_message = (char *)CipCalloc(1,kErrorMessageBufferSize);
    if(error_message == NULL)
        return NULL;

    snprintf(error_message, kErrorMessageBufferSize,
             "%s (%d)", error_name, error_number);

    return error_message;
}

void FreeErrorMessage(char *error_message)
{
    CipFree(error_message);
}
