#ifndef P101_DOCTOR_STATUS_H
#define P101_DOCTOR_STATUS_H

#include <p101_env/env.h>
#include <p101_error/error.h>
#include <stdbool.h>
#include <stdio.h>

bool        p101_doctor_status_is_clean(int status);
bool        p101_doctor_status_has_findings(int status);
bool        p101_doctor_status_is_acceptable(int status);
const char *p101_doctor_status_word(int status);
const char *p101_doctor_status_grade(int status);
void        p101_doctor_print_status_markdown(const struct p101_env *env, struct p101_error *err, FILE *stream, const char *label, int status);
void        p101_doctor_print_status_json(const struct p101_env *env, struct p101_error *err, FILE *stream, const char *label, int status);

#endif    // P101_DOCTOR_STATUS_H
