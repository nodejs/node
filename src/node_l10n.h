#ifndef SRC_NODE_L10N_H_
#define SRC_NODE_L10N_H_

/**
 * Defines a handful of macros:
 *
 * - L10N(key,fallback)
 * - L10N_LOCALE()
 * - L10N_PRINT(key, fallback)
 * - L10N_PRINTF(key, fallback, ...)
 * - L10N_INIT(locale, icu_data_dir)
 *
 * When --with-intl=full-icu, this will use the ICU message bundle
 * mechanism. In all other cases, this uses the simple fallback.
 * This will minimize impact for default builds but will allow
 * node to be fully localized when using the full ICU dataset. 
 **/

#if defined(NODE_HAVE_I18N_SUPPORT)
#if !defined(NODE_HAVE_SMALL_ICU)
  // only enabled with --with-intl=full-icu
  #include <l10n.h>

  #define L10N_PRINT(key, fallback)                                           \
    do {                                                                      \
      UChar *str;                                                             \
      UErrorCode status = U_ZERO_ERROR;                                       \
      str=(UChar*)malloc(sizeof(UChar) * (strlen(fallback) +1));              \
      u_uastrcpy(str, fallback);                                              \
      u_printf_u(L10N(key, str));                                             \
      free(str);                                                              \
    } while(0)

  #define L10N_PRINTF(key, fallback, ...)                                     \
    do {                                                                      \
      UChar *str;                                                             \
      UErrorCode status = U_ZERO_ERROR;                                       \
      str=(UChar*)malloc(sizeof(UChar) * (strlen(fallback) +1));              \
      u_uastrcpy(str, fallback);                                              \
      u_printf_u(L10N(key, str),__VA_ARGS__);                                 \
      free(str);                                                              \
    } while(0)
#endif
#endif

#ifndef L10N
  // not using full-icu
  #define L10N(key, fallback) fallback
  #define L10N_LOCALE() "en_US"
  #define L10N_INIT(locale, icu_data_dir) do {} while(0)
  #define L10N_PRINT(key, fallback) printf(fallback)
  #define L10N_PRINTF(key, fallback, ...) printf(fallback, __VA_ARGS__)
#endif

#include "env.h"

namespace node {

using v8::Local;
using v8::Object;
using v8::Value;
using v8::Context;
using v8::FunctionCallbackInfo;

namespace l10n {

  static void Initialize(Local<Object> target,
                         Local<Value> unused,
                         Local<Context> context);

  static void Fetch(const FunctionCallbackInfo<Value>& args);

} // namespace l10n
} // namespace node

#endif // SRC_NODE_L10N_H_
