/**
 * @file ada_c.h
 * @brief Includes the C definitions for Ada. This is a C file, not C++.
 */
#ifndef ADA_C_H
#define ADA_C_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

// This is a reference to ada::url_components::omitted
// It represents "uint32_t(-1)"
#define ada_url_omitted 0xffffffff

// string that is owned by the ada_url instance
typedef struct {
  const char* data;
  size_t length;
} ada_string;

// string that must be freed by the caller
typedef struct {
  const char* data;
  size_t length;
} ada_owned_string;

typedef struct {
  uint32_t protocol_end;
  uint32_t username_end;
  uint32_t host_start;
  uint32_t host_end;
  uint32_t port;
  uint32_t pathname_start;
  uint32_t search_start;
  uint32_t hash_start;
} ada_url_components;

typedef void* ada_url;

// input should be a null terminated C string (ASCII or UTF-8)
// you must call ada_free on the returned pointer
ada_url ada_parse(const char* input, size_t length);
ada_url ada_parse_with_base(const char* input, size_t input_length,
                            const char* base, size_t base_length);

// input and base should be a null terminated C strings
bool ada_can_parse(const char* input, size_t length);
bool ada_can_parse_with_base(const char* input, size_t input_length,
                             const char* base, size_t base_length);

void ada_free(ada_url result);
void ada_free_owned_string(ada_owned_string owned);
ada_url ada_copy(ada_url input);

bool ada_is_valid(ada_url result);

// url_aggregator getters
// if ada_is_valid(result)) is false, an empty string is returned
ada_owned_string ada_get_origin(ada_url result);
ada_string ada_get_href(ada_url result);
ada_string ada_get_username(ada_url result);
ada_string ada_get_password(ada_url result);
ada_string ada_get_port(ada_url result);
ada_string ada_get_hash(ada_url result);
ada_string ada_get_host(ada_url result);
ada_string ada_get_hostname(ada_url result);
ada_string ada_get_pathname(ada_url result);
ada_string ada_get_search(ada_url result);
ada_string ada_get_protocol(ada_url result);
uint8_t ada_get_url_host_type(ada_url result);

// url_aggregator setters
// if ada_is_valid(result)) is false, the setters have no effect
// input should be a null terminated C string
bool ada_set_href(ada_url result, const char* input, size_t length);
bool ada_set_host(ada_url result, const char* input, size_t length);
bool ada_set_hostname(ada_url result, const char* input, size_t length);
bool ada_set_protocol(ada_url result, const char* input, size_t length);
bool ada_set_username(ada_url result, const char* input, size_t length);
bool ada_set_password(ada_url result, const char* input, size_t length);
bool ada_set_port(ada_url result, const char* input, size_t length);
bool ada_set_pathname(ada_url result, const char* input, size_t length);
void ada_set_search(ada_url result, const char* input, size_t length);
void ada_set_hash(ada_url result, const char* input, size_t length);

// url_aggregator functions
// if ada_is_valid(result) is false, functions below will return false
bool ada_has_credentials(ada_url result);
bool ada_has_empty_hostname(ada_url result);
bool ada_has_hostname(ada_url result);
bool ada_has_non_empty_username(ada_url result);
bool ada_has_non_empty_password(ada_url result);
bool ada_has_port(ada_url result);
bool ada_has_password(ada_url result);
bool ada_has_hash(ada_url result);
bool ada_has_search(ada_url result);

// returns a pointer to the internal url_aggregator::url_components
const ada_url_components* ada_get_components(ada_url result);

// idna methods
ada_owned_string ada_idna_to_unicode(const char* input, size_t length);
ada_owned_string ada_idna_to_ascii(const char* input, size_t length);

#endif  // ADA_C_H
