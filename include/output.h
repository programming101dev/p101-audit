#ifndef P101_WRAPPER_AUDIT_OUTPUT_H
#define P101_WRAPPER_AUDIT_OUTPUT_H

#include "model.h"

void p101_wrapper_output_json_string(const struct p101_env *env, struct p101_error *err, FILE *stream, const char *text);
const char *p101_wrapper_output_json_bool_text(bool value);
#include <stdio.h>

void p101_wrapper_write_audit(const struct p101_env *env, struct p101_error *err, const struct p101_wrapper_model *model, const struct p101_wrapper_arguments *arguments);
void p101_wrapper_write_inventory(const struct p101_env *env, struct p101_error *err, const struct p101_wrapper_model *model, bool json);
void p101_wrapper_write_facts(const struct p101_env *env, struct p101_error *err, const struct p101_wrapper_model *model, FILE *stream);
void p101_wrapper_write_diagnostics(const struct p101_env *env, struct p101_error *err, const struct p101_wrapper_model *model, FILE *stream);
bool p101_wrapper_write_optional_outputs(const struct p101_env *env, struct p101_error *err, const struct p101_wrapper_model *model, const struct p101_wrapper_arguments *arguments);

#endif
