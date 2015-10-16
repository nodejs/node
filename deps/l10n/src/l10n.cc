#include <stdlib.h>
#include <unicode/udata.h>
#include <unicode/ures.h>
#include "l10n.h"

// The ICU bundle name
#define L10N_APPDATA "node"

extern "C" const char U_DATA_API node_dat[];

UResourceBundle *bundle;

void l10n_cleanup() {
  ures_close(bundle);
}

bool l10n_initialize(const char * locale,
                     const char * icu_data_dir) {
  UErrorCode status = U_ZERO_ERROR;
  if (!icu_data_dir) {
    udata_setAppData("node", &node_dat, &status);
  }
  if (U_FAILURE(status)) {
    return FALSE;
  } else {
    bundle = ures_open(L10N_APPDATA, locale, &status);
    if (U_SUCCESS(status)) {
      atexit(l10n_cleanup);
      return TRUE;
    } else {
      return FALSE;
    }
  }
}

const uint16_t * l10n_fetch(const char * key,
                            const uint16_t * fallback) {
  UErrorCode status = U_ZERO_ERROR;
  int32_t len = 0;
  const UChar* res = ures_getStringByKey(
                          bundle,
                          key,
                          &len,
                          &status);
  const uint16_t* ret = static_cast<const uint16_t*>(res);
  if (U_SUCCESS(status)) {
    return ret;
  }
  return fallback;
}
