/*
**********************************************************************
*   Copyright (C) 2014, International Business Machines
*   Corporation and others.  All Rights Reserved.
**********************************************************************
*
* Created 2014-06-20 by Steven R. Loomis
*
* See: http://bugs.icu-project.org/trac/ticket/10922
*
*/

/*
WHAT IS THIS?

Here's the problem: It's difficult to reconfigure ICU from the command
line without using the full makefiles. You can do a lot, but not
everything.

Consider:

 $ icupkg -r 'ja*' icudt53l.dat

Great, you've now removed the (main) Japanese data. But something's
still wrong-- res_index (and thus, getAvailable* functions) still
claim the locale is present.

You are reading the source to a tool (using only public API C code)
that can solve this problem. Use as follows:

 $ iculslocs -i . -N icudt53l -b res_index.txt

.. Generates a NEW res_index.txt (by looking at the .dat file, and
figuring out which locales are actually available. Has commented out
the ones which are no longer available:

          ...
          it_SM {""}
//        ja {""}
//        ja_JP {""}
          jgo {""}
          ...

Then you can build and in-place patch it with existing ICU tools:
 $ genrb res_index.txt
 $ icupkg -a res_index.res icudt53l.dat

.. Now you have a patched icudt539.dat that not only doesn't have
Japanese, it doesn't *claim* to have Japanese.

*/

#include "string.h"
#include "charstr.h"  // ICU internal header
#include <unicode/ustdio.h>
#include <unicode/ures.h>
#include <unicode/udata.h>

const char* PROG = "iculslocs";
const char* NAME = U_ICUDATA_NAME;  // assume ICU data
const char* TREE = "ROOT";
int VERBOSE = 0;

#define RES_INDEX "res_index"
#define INSTALLEDLOCALES "InstalledLocales"

CharString packageName;
const char* locale = RES_INDEX;  // locale referring to our index

void usage() {
  u_printf("Usage: %s [options]\n", PROG);
  u_printf(
      "This program lists and optionally regenerates the locale "
      "manifests\n"
      " in ICU 'res_index.res' files.\n");
  u_printf(
      "  -i ICUDATA  Set ICUDATA dir to ICUDATA.\n"
      "    NOTE: this must be the first option given.\n");
  u_printf("  -h          This Help\n");
  u_printf("  -v          Verbose Mode on\n");
  u_printf("  -l          List locales to stdout\n");
  u_printf(
      "               if Verbose mode, then missing (unopenable)"
      "locales\n"
      "               will be listed preceded by a '#'.\n");
  u_printf(
      "  -b res_index.txt  Write 'corrected' bundle "
      "to res_index.txt\n"
      "                    missing bundles will be "
      "OMITTED\n");
  u_printf(
      "  -T TREE     Choose tree TREE\n"
      "         (TREE should be one of: \n"
      "    ROOT, brkitr, coll, curr, lang, rbnf, region, zone)\n");
  // see ureslocs.h and elsewhere
  u_printf(
      "  -N NAME     Choose name NAME\n"
      "         (default: '%s')\n",
      U_ICUDATA_NAME);
  u_printf(
      "\nNOTE: for best results, this tool ought to be "
      "linked against\n"
      "stubdata. i.e. '%s -l' SHOULD return an error with "
      " no data.\n",
      PROG);
}

#define ASSERT_SUCCESS(status, what)      \
  if (U_FAILURE(*status)) {               \
    u_printf("%s:%d: %s: ERROR: %s %s\n", \
             __FILE__,                    \
             __LINE__,                    \
             PROG,                        \
             u_errorName(*status),        \
             what);                       \
    return 1;                             \
  }

/**
 * @param status changed from reference to pointer to match node.js style
 */
void calculatePackageName(UErrorCode* status) {
  packageName.clear();
  if (strcmp(NAME, "NONE")) {
    packageName.append(NAME, *status);
    if (strcmp(TREE, "ROOT")) {
      packageName.append(U_TREE_SEPARATOR_STRING, *status);
      packageName.append(TREE, *status);
    }
  }
  if (VERBOSE) {
    u_printf("packageName: %s\n", packageName.data());
  }
}

/**
 * Does the locale exist?
 * return zero for false, or nonzero if it was openable.
 * Assumes calculatePackageName was called.
 * @param exists set to TRUE if exists, FALSE otherwise.
 * Changed from reference to pointer to match node.js style
 * @return 0 on "OK" (success or resource-missing),
 * 1 on "FAILURE" (unexpected error)
 */
int localeExists(const char* loc, UBool* exists) {
  UErrorCode status = U_ZERO_ERROR;
  if (VERBOSE > 1) {
    u_printf("Trying to open %s:%s\n", packageName.data(), loc);
  }
  LocalUResourceBundlePointer aResource(
      ures_openDirect(packageName.data(), loc, &status));
  *exists = FALSE;
  if (U_SUCCESS(status)) {
    *exists = true;
    if (VERBOSE > 1) {
      u_printf("%s:%s existed!\n", packageName.data(), loc);
    }
    return 0;
  } else if (status == U_MISSING_RESOURCE_ERROR) {
    *exists = false;
    if (VERBOSE > 1) {
      u_printf("%s:%s did NOT exist (%s)!\n",
               packageName.data(),
               loc,
               u_errorName(status));
    }
    return 0;  // "good" failure
  } else {
    // some other failure..
    u_printf("%s:%d: %s: ERROR %s opening %s:%s for test.\n",
             __FILE__,
             __LINE__,
             u_errorName(status),
             packageName.data(),
             loc);
    return 1;  // abort
  }
}

void printIndent(const LocalUFILEPointer* bf, int indent) {
  for (int i = 0; i < indent + 1; i++) {
    u_fprintf(bf->getAlias(), "    ");
  }
}

/**
 * Dumps a table resource contents
 * if lev==0, skips INSTALLEDLOCALES
 * @return 0 for OK, 1 for err
 */
int dumpAllButInstalledLocales(int lev,
                               LocalUResourceBundlePointer* bund,
                               LocalUFILEPointer* bf,
                               UErrorCode* status) {
  ures_resetIterator(bund->getAlias());
  LocalUResourceBundlePointer t;
  while (U_SUCCESS(*status) && ures_hasNext(bund->getAlias())) {
    t.adoptInstead(ures_getNextResource(bund->getAlias(), t.orphan(), status));
    ASSERT_SUCCESS(status, "while processing table");
    const char* key = ures_getKey(t.getAlias());
    if (VERBOSE > 1) {
      u_printf("dump@%d: got key %s\n", lev, key);
    }
    if (lev == 0 && !strcmp(key, INSTALLEDLOCALES)) {
      if (VERBOSE > 1) {
        u_printf("dump: skipping '%s' as it must be evaluated.\n", key);
      }
    } else {
      printIndent(bf, lev);
      u_fprintf(bf->getAlias(), "%s", key);
      switch (ures_getType(t.getAlias())) {
        case URES_STRING: {
          int32_t len = 0;
          const UChar* s = ures_getString(t.getAlias(), &len, status);
          ASSERT_SUCCESS(status, "getting string");
          u_fprintf(bf->getAlias(), ":string {\"");
          u_file_write(s, len, bf->getAlias());
          u_fprintf(bf->getAlias(), "\"}");
        } break;
        default: {
          u_printf("ERROR: unhandled type in dumpAllButInstalledLocales().\n");
          return 1;
        } break;
      }
      u_fprintf(bf->getAlias(), "\n");
    }
  }
  return 0;
}

int list(const char* toBundle) {
  UErrorCode status = U_ZERO_ERROR;

  LocalUFILEPointer bf;

  if (toBundle != NULL) {
    if (VERBOSE) {
      u_printf("writing to bundle %s\n", toBundle);
    }
    // we write UTF-8 with BOM only. No exceptions.
    bf.adoptInstead(u_fopen(toBundle, "w", "en_US_POSIX", "UTF-8"));
    if (bf.isNull()) {
      u_printf("ERROR: Could not open '%s' for writing.\n", toBundle);
      return 1;
    }
    u_fputc(0xFEFF, bf.getAlias());  // write BOM
    u_fprintf(bf.getAlias(), "// -*- Coding: utf-8; -*-\n//\n");
  }

  // first, calculate the bundle name.
  calculatePackageName(&status);
  ASSERT_SUCCESS(&status, "calculating package name");

  if (VERBOSE) {
    u_printf("\"locale\": %s\n", locale);
  }

  LocalUResourceBundlePointer bund(
      ures_openDirect(packageName.data(), locale, &status));
  ASSERT_SUCCESS(&status, "while opening the bundle");
  LocalUResourceBundlePointer installedLocales(
      ures_getByKey(bund.getAlias(), INSTALLEDLOCALES, NULL, &status));
  ASSERT_SUCCESS(&status, "while fetching installed locales");

  int32_t count = ures_getSize(installedLocales.getAlias());
  if (VERBOSE) {
    u_printf("Locales: %d\n", count);
  }

  if (bf.isValid()) {
    // write the HEADER
    u_fprintf(bf.getAlias(),
              "// Warning this file is automatically generated\n"
              "// Updated by %s based on %s:%s.txt\n",
              PROG,
              packageName.data(),
              locale);
    u_fprintf(bf.getAlias(),
              "%s:table(nofallback) {\n"
              "    // First, everything besides InstalledLocales:\n",
              locale);
    if (dumpAllButInstalledLocales(0, &bund, &bf, &status)) {
      u_printf("Error dumping prolog for %s\n", toBundle);
      return 1;
    }
    // in case an error was missed
    ASSERT_SUCCESS(&status, "while writing prolog");

    u_fprintf(bf.getAlias(),
              "    %s:table { // %d locales in input %s.res\n",
              INSTALLEDLOCALES,
              count,
              locale);
  }

  // OK, now list them.
  LocalUResourceBundlePointer subkey;

  int validCount = 0;
  for (int32_t i = 0; i < count; i++) {
    subkey.adoptInstead(ures_getByIndex(
        installedLocales.getAlias(), i, subkey.orphan(), &status));
    ASSERT_SUCCESS(&status, "while fetching an installed locale's name");

    const char* key = ures_getKey(subkey.getAlias());
    if (VERBOSE > 1) {
      u_printf("@%d: %s\n", i, key);
    }
    // now, see if the locale is installed..

    UBool exists;
    if (localeExists(key, &exists)) {
      return 1;  // get out.
    }
    if (exists) {
      validCount++;
      u_printf("%s\n", key);
      if (bf.isValid()) {
        u_fprintf(bf.getAlias(), "        %s {\"\"}\n", key);
      }
    } else {
      if (bf.isValid()) {
        u_fprintf(bf.getAlias(), "//      %s {\"\"}\n", key);
      }
      if (VERBOSE) {
        u_printf("#%s\n", key);  // verbosity one - '' vs '#'
      }
    }
  }

  if (bf.isValid()) {
    u_fprintf(bf.getAlias(), "    } // %d/%d valid\n", validCount, count);
    // write the HEADER
    u_fprintf(bf.getAlias(), "}\n");
  }
  return 0;
}

int main(int argc, const char* argv[]) {
  PROG = argv[0];
  for (int i = 1; i < argc; i++) {
    const char* arg = argv[i];
    int argsLeft = argc - i - 1; /* how many remain? */
    if (!strcmp(arg, "-v")) {
      VERBOSE++;
    } else if (!strcmp(arg, "-i") && (argsLeft >= 1)) {
      if (i != 1) {
        u_printf("ERROR: -i must be the first argument given.\n");
        usage();
        return 1;
      }
      const char* dir = argv[++i];
      u_setDataDirectory(dir);
      if (VERBOSE) {
        u_printf("ICUDATA is now %s\n", dir);
      }
    } else if (!strcmp(arg, "-T") && (argsLeft >= 1)) {
      TREE = argv[++i];
      if (VERBOSE) {
        u_printf("TREE is now %s\n", TREE);
      }
    } else if (!strcmp(arg, "-N") && (argsLeft >= 1)) {
      NAME = argv[++i];
      if (VERBOSE) {
        u_printf("NAME is now %s\n", NAME);
      }
    } else if (!strcmp(arg, "-?") || !strcmp(arg, "-h")) {
      usage();
      return 0;
    } else if (!strcmp(arg, "-l")) {
      if (list(NULL)) {
        return 1;
      }
    } else if (!strcmp(arg, "-b") && (argsLeft >= 1)) {
      if (list(argv[++i])) {
        return 1;
      }
    } else {
      u_printf("Unknown or malformed option: %s\n", arg);
      usage();
      return 1;
    }
  }
}

// Local Variables:
// compile-command: "icurun iculslocs.cpp"
// End:
