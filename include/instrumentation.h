#ifndef P101_AUDIT_INSTRUMENTATION_H
#define P101_AUDIT_INSTRUMENTATION_H

#include "model.h"

struct p101_instrumentation_capabilities
{
    bool trace_entry;
    bool trace_exit;
    bool fault;
    bool fd;
    bool allocation;
    bool resource;
};

bool p101_instrumentation_collect(const struct p101_env *env, struct p101_error *err, const struct p101_wrapper_model *model, struct p101_instrumentation_capabilities *capabilities);

#endif
