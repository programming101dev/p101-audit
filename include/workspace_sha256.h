#ifndef P101_AUDIT_WORKSPACE_SHA256_H
#define P101_AUDIT_WORKSPACE_SHA256_H

#include <stddef.h>

enum
{
    P101_WORKSPACE_SHA256_TEXT_SIZE = 65
};

void p101_workspace_sha256(const unsigned char *data, size_t length, char output[P101_WORKSPACE_SHA256_TEXT_SIZE]);

#endif
