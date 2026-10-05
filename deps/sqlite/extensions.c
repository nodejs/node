#include "sqlite3.h"

/* Build and automatically load the same set of default extensions as the CLI     */
/* The list of extensions can be found in sqlite's shell.c.in, function open_db() */

#include "ext/misc/sha1.c"
#include "ext/misc/shathree.c"
#include "ext/misc/uint.c"
#include "ext/misc/stmtrand.c"
#include "ext/misc/decimal.c"
#include "ext/misc/base64.c"
#include "ext/misc/base85.c"
#include "ext/misc/regexp.c"
#include "ext/misc/ieee754.c"
#include "ext/misc/series.c"

void node_sqlite_static_extensions_init() {
  sqlite3_auto_extension((void*)sqlite3_sha_init);
  sqlite3_auto_extension((void*)sqlite3_shathree_init);
  sqlite3_auto_extension((void*)sqlite3_uint_init);
  sqlite3_auto_extension((void*)sqlite3_stmtrand_init);
  sqlite3_auto_extension((void*)sqlite3_decimal_init);
  sqlite3_auto_extension((void*)sqlite3_base64_init);
  sqlite3_auto_extension((void*)sqlite3_base85_init);
  sqlite3_auto_extension((void*)sqlite3_regexp_init);
  sqlite3_auto_extension((void*)sqlite3_ieee_init);
  sqlite3_auto_extension((void*)sqlite3_series_init);
}
