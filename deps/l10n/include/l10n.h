#ifndef L10N__H
#define L10N__H

#include <unicode/uloc.h>
#include <unicode/ustdio.h>
#include <unicode/ustring.h>

#ifdef _WIN32
# define L10N_EXTERN /* nothing */
#elif __GNUC__ >= 4
# define L10N_EXTERN __attribute__((visibility("default")))
#else
# define L10N_EXTERN /* nothing */
#endif

/**
 * Initialize the resource bundle. This will register an atexit handler
 * to deal with the cleanup in normal termination
 **/
L10N_EXTERN bool l10n_initialize(const char * locale, const char * icu_data_dir);

/**
 * Lookup the key, return the value. Doesn't get much easier. If the key
 * is not found in the bundle, fallback is returned instead. The caller
 * owns the string and must delete[] it when done lest horrible things.
 **/
L10N_EXTERN const uint16_t * l10n_fetch(const char * key,
                                        const uint16_t * fallback);

#define L10N(key, fallback) l10n_fetch(key, fallback)
#define L10N_LOCALE() uloc_getDefault()
#define L10N_INIT(locale, icu_data_dir)                                       \
  do {l10n_initialize(locale, icu_data_dir);} while(0)

#endif // L10N__H
