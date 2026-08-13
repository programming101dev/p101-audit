#ifndef P101_AUDIT_WORKSPACE_SHA256_H
#define P101_AUDIT_WORKSPACE_SHA256_H

#include <stddef.h>

void p101_workspace_sha256(const unsigned char *data, size_t length, char output[65]);

#endif
