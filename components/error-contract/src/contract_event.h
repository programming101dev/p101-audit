#ifndef P101_ERROR_CONTRACT_EVENT_H
#define P101_ERROR_CONTRACT_EVENT_H

#include "../include/contract_types.h"
#include <p101_c_facts/facts.h>
#include <stdbool.h>

bool p101_contract_event_needs_env(const struct contract_event *event);
bool p101_contract_event_needs_error(const struct contract_event *event);
bool p101_contract_event_is_not_after(const struct contract_event *candidate, const struct contract_event *event);
bool p101_contract_event_kind_from_note(enum p101_c_note_kind note, enum contract_event_kind *kind);

#endif
