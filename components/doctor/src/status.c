#include "../include/status.h"
#include "../include/constants.h"
#include <p101_c/p101_stdio.h>
#include <stdlib.h>
#include <sys/wait.h>

bool p101_doctor_status_is_clean(int status)
{
    return (WIFEXITED(status) && WEXITSTATUS(status) == EXIT_SUCCESS) != 0;
}

bool p101_doctor_status_has_findings(int status)
{
    return (WIFEXITED(status) && WEXITSTATUS(status) == EXIT_FINDINGS) != 0;
}

bool p101_doctor_status_is_acceptable(int status)
{
    int  p101_expression_result_5;
    bool p101_call_result_6;
    p101_call_result_6 = p101_doctor_status_is_clean(status);
    if(p101_call_result_6)
    {
        p101_expression_result_5 = 1;
    }
    else
    {
        bool p101_call_result_7;

        p101_call_result_7 = p101_doctor_status_has_findings(status);
        if(p101_call_result_7)
        {
            p101_expression_result_5 = 1;
        }
        else
        {
            p101_expression_result_5 = 0;
        }
    }
    return p101_expression_result_5 != 0;
}

/*
 * The single classification of a child exit status. Every human-facing word
 * (the status word and the report grade) is derived from this one cascade so
 * the two vocabularies can never drift apart.
 */
enum doctor_status_class
{
    DOCTOR_STATUS_CLASS_CLEAN = 0,
    DOCTOR_STATUS_CLASS_FINDINGS,
    DOCTOR_STATUS_CLASS_TROUBLE
};

static enum doctor_status_class classify_status(int status);

static enum doctor_status_class classify_status(int status)
{
    bool p101_call_result_8;
    enum doctor_status_class class;

    class = DOCTOR_STATUS_CLASS_TROUBLE;

    p101_call_result_8 = p101_doctor_status_is_clean(status);
    if(p101_call_result_8)
    {
        class = DOCTOR_STATUS_CLASS_CLEAN;
    }
    else
    {
        bool p101_call_result_9;

        p101_call_result_9 = p101_doctor_status_has_findings(status);
        if(p101_call_result_9)
        {
            class = DOCTOR_STATUS_CLASS_FINDINGS;
        }
    }

    return class;
}

const char *p101_doctor_status_word(int status)
{
    enum doctor_status_class p101_call_result_1;
    const char              *word;

    p101_call_result_1 = classify_status(status);
    word               = "trouble";
    if(p101_call_result_1 == DOCTOR_STATUS_CLASS_CLEAN)
    {
        word = "clean";
    }
    else if(p101_call_result_1 == DOCTOR_STATUS_CLASS_FINDINGS)
    {
        word = "findings";
    }

    return word;
}

const char *p101_doctor_status_grade(int status)
{
    enum doctor_status_class p101_call_result_10;
    const char              *grade;

    p101_call_result_10 = classify_status(status);
    grade               = "trouble";
    if(p101_call_result_10 == DOCTOR_STATUS_CLASS_CLEAN)
    {
        grade = "good";
    }
    else if(p101_call_result_10 == DOCTOR_STATUS_CLASS_FINDINGS)
    {
        grade = "needs work";
    }

    return grade;
}

void p101_doctor_print_status_markdown(const struct p101_env *env, struct p101_error *err, FILE *stream, const char *label, int status)
{
    if(WIFEXITED(status))
    {
        const char *p101_call_result_2;

        p101_call_result_2 = p101_doctor_status_word(status);
        p101_fprintf(env, err, stream, "| %s | %s (exit %d) |\n", label, p101_call_result_2, WEXITSTATUS(status));
    }
    else if(WIFSTOPPED(status))
    {
        p101_fprintf(env, err, stream, "| %s | trouble (status %d) |\n", label, status);
    }
    else
    {
        p101_fprintf(env, err, stream, "| %s | trouble (signal %d) |\n", label, WTERMSIG(status));
    }
}

void p101_doctor_print_status_json(const struct p101_env *env, struct p101_error *err, FILE *stream, const char *label, int status)
{
    if(WIFEXITED(status))
    {
        const char *p101_call_result_3;

        p101_call_result_3 = p101_doctor_status_word(status);
        p101_fprintf(env, err, stream, "    \"%s\": {\"kind\": \"exit\", \"code\": %d, \"result\": \"%s\"}", label, WEXITSTATUS(status), p101_call_result_3);
    }
    else if(WIFSTOPPED(status))
    {
        p101_fprintf(env, err, stream, "    \"%s\": {\"kind\": \"status\", \"status\": %d, \"result\": \"trouble\"}", label, status);
    }
    else
    {
        p101_fprintf(env, err, stream, "    \"%s\": {\"kind\": \"signal\", \"signal\": %d, \"result\": \"trouble\"}", label, WTERMSIG(status));
    }
}
