// Copyright (C) 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
**********************************************************************
*   Copyright (C) 1997-2016, International Business Machines
*   Corporation and others.  All Rights Reserved.
**********************************************************************
*
* File ULOC.CPP
*
* Modification History:
*
*   Date        Name        Description
*   04/01/97    aliu        Creation.
*   08/21/98    stephen     JDK 1.2 sync
*   12/08/98    rtg         New Locale implementation and C API
*   03/15/99    damiba      overhaul.
*   04/06/99    stephen     changed setDefault() to realloc and copy
*   06/14/99    stephen     Changed calls to ures_open for new params
*   07/21/99    stephen     Modified setDefault() to propagate to C++
*   05/14/04    alan        7 years later: refactored, cleaned up, fixed bugs,
*                           brought canonicalization code into line with spec
*****************************************************************************/

/*
   POSIX's locale format, from putil.c: [no spaces]

     ll [ _CC ] [ . MM ] [ @ VV]

     l = lang, C = ctry, M = charmap, V = variant
*/

#include "unicode/utypes.h"
#include "unicode/ustring.h"
#include "unicode/uloc.h"

#include "putilimp.h"
#include "ustr_imp.h"
#include "ulocimp.h"
#include "umutex.h"
#include "cstring.h"
#include "cmemory.h"
#include "locmap.h"
#include "uarrsort.h"
#include "uenumimp.h"
#include "uassert.h"

#include <stdio.h> /* for sprintf */

using namespace icu;

/* ### Declarations **************************************************/

/* Locale stuff from locid.cpp */
U_CFUNC void locale_set_default(const char *id);
U_CFUNC const char *locale_get_default(void);
U_CFUNC int32_t
locale_getKeywords(const char *localeID,
            char prev,
            char *keywords, int32_t keywordCapacity,
            char *values, int32_t valuesCapacity, int32_t *valLen,
            UBool valuesToo,
            UErrorCode *status);

/* ### Data tables **************************************************/

/**
 * Table of language codes, both 2- and 3-letter, with preference
 * given to 2-letter codes where possible.  Includes 3-letter codes
 * that lack a 2-letter equivalent.
 *
 * This list must be in sorted order.  This list is returned directly
 * to the user by some API.
 *
 * This list must be kept in sync with LANGUAGES_3, with corresponding
 * entries matched.
 *
 * This table should be terminated with a NULL entry, followed by a
 * second list, and another NULL entry.  The first list is visible to
 * user code when this array is returned by API.  The second list
 * contains codes we support, but do not expose through user API.
 *
 * Notes
 *
 * Tables updated per http://lcweb.loc.gov/standards/iso639-2/ to
 * include the revisions up to 2001/7/27 *CWB*
 *
 * The 3 character codes are the terminology codes like RFC 3066.  This
 * is compatible with prior ICU codes
 *
 * "in" "iw" "ji" "jw" & "sh" have been withdrawn but are still in the
 * table but now at the end of the table because 3 character codes are
 * duplicates.  This avoids bad searches going from 3 to 2 character
 * codes.
 *
 * The range qaa-qtz is reserved for local use
 */
/* Generated using org.unicode.cldr.icu.GenerateISO639LanguageTables */
/* ISO639 table version is 20150505 */
static const char * const LANGUAGES[] = {
    "aa",  "ab",  "ace", "ach", "ada", "ady", "ae",  "aeb",
    "af",  "afh", "agq", "ain", "ak",  "akk", "akz", "ale",
    "aln", "alt", "am",  "an",  "ang", "anp", "ar",  "arc",
    "arn", "aro", "arp", "arq", "arw", "ary", "arz", "as",
    "asa", "ase", "ast", "av",  "avk", "awa", "ay",  "az",
    "ba",  "bal", "ban", "bar", "bas", "bax", "bbc", "bbj",
    "be",  "bej", "bem", "bew", "bez", "bfd", "bfq", "bg",
    "bgn", "bho", "bi",  "bik", "bin", "bjn", "bkm", "bla",
    "bm",  "bn",  "bo",  "bpy", "bqi", "br",  "bra", "brh",
    "brx", "bs",  "bss", "bua", "bug", "bum", "byn", "byv",
    "ca",  "cad", "car", "cay", "cch", "ce",  "ceb", "cgg",
    "ch",  "chb", "chg", "chk", "chm", "chn", "cho", "chp",
    "chr", "chy", "ckb", "co",  "cop", "cps", "cr",  "crh",
    "cs",  "csb", "cu",  "cv",  "cy",
    "da",  "dak", "dar", "dav", "de",  "del", "den", "dgr",
    "din", "dje", "doi", "dsb", "dtp", "dua", "dum", "dv",
    "dyo", "dyu", "dz",  "dzg",
    "ebu", "ee",  "efi", "egl", "egy", "eka", "el",  "elx",
    "en",  "enm", "eo",  "es",  "esu", "et",  "eu",  "ewo",
    "ext",
    "fa",  "fan", "fat", "ff",  "fi",  "fil", "fit", "fj",
    "fo",  "fon", "fr",  "frc", "frm", "fro", "frp", "frr",
    "frs", "fur", "fy",
    "ga",  "gaa", "gag", "gan", "gay", "gba", "gbz", "gd",
    "gez", "gil", "gl",  "glk", "gmh", "gn",  "goh", "gom",
    "gon", "gor", "got", "grb", "grc", "gsw", "gu",  "guc",
    "gur", "guz", "gv",  "gwi",
    "ha",  "hai", "hak", "haw", "he",  "hi",  "hif", "hil",
    "hit", "hmn", "ho",  "hr",  "hsb", "hsn", "ht",  "hu",
    "hup", "hy",  "hz",
    "ia",  "iba", "ibb", "id",  "ie",  "ig",  "ii",  "ik",
    "ilo", "inh", "io",  "is",  "it",  "iu",  "izh",
    "ja",  "jam", "jbo", "jgo", "jmc", "jpr", "jrb", "jut",
    "jv",
    "ka",  "kaa", "kab", "kac", "kaj", "kam", "kaw", "kbd",
    "kbl", "kcg", "kde", "kea", "ken", "kfo", "kg",  "kgp",
    "kha", "kho", "khq", "khw", "ki",  "kiu", "kj",  "kk",
    "kkj", "kl",  "kln", "km",  "kmb", "kn",  "ko",  "koi",
    "kok", "kos", "kpe", "kr",  "krc", "kri", "krj", "krl",
    "kru", "ks",  "ksb", "ksf", "ksh", "ku",  "kum", "kut",
    "kv",  "kw",  "ky",
    "la",  "lad", "lag", "lah", "lam", "lb",  "lez", "lfn",
    "lg",  "li",  "lij", "liv", "lkt", "lmo", "ln",  "lo",
    "lol", "loz", "lrc", "lt",  "ltg", "lu",  "lua", "lui",
    "lun", "luo", "lus", "luy", "lv",  "lzh", "lzz",
    "mad", "maf", "mag", "mai", "mak", "man", "mas", "mde",
    "mdf", "mdh", "mdr", "men", "mer", "mfe", "mg",  "mga",
    "mgh", "mgo", "mh",  "mi",  "mic", "min", "mis", "mk",
    "ml",  "mn",  "mnc", "mni", "moh", "mos", "mr",  "mrj",
    "ms",  "mt",  "mua", "mul", "mus", "mwl", "mwr", "mwv",
    "my",  "mye", "myv", "mzn",
    "na",  "nan", "nap", "naq", "nb",  "nd",  "nds", "ne",
    "new", "ng",  "nia", "niu", "njo", "nl",  "nmg", "nn",
    "nnh", "no",  "nog", "non", "nov", "nqo", "nr",  "nso",
    "nus", "nv",  "nwc", "ny",  "nym", "nyn", "nyo", "nzi",
    "oc",  "oj",  "om",  "or",  "os",  "osa", "ota",
    "pa",  "pag", "pal", "pam", "pap", "pau", "pcd", "pdc",
    "pdt", "peo", "pfl", "phn", "pi",  "pl",  "pms", "pnt",
    "pon", "prg", "pro", "ps",  "pt",
    "qu",  "quc", "qug",
    "raj", "rap", "rar", "rgn", "rif", "rm",  "rn",  "ro",
    "rof", "rom", "rtm", "ru",  "rue", "rug", "rup",
    "rw",  "rwk",
    "sa",  "sad", "sah", "sam", "saq", "sas", "sat", "saz",
    "sba", "sbp", "sc",  "scn", "sco", "sd",  "sdc", "sdh",
    "se",  "see", "seh", "sei", "sel", "ses", "sg",  "sga",
    "sgs", "shi", "shn", "shu", "si",  "sid", "sk",
    "sl",  "sli", "sly", "sm",  "sma", "smj", "smn", "sms",
    "sn",  "snk", "so",  "sog", "sq",  "sr",  "srn", "srr",
    "ss",  "ssy", "st",  "stq", "su",  "suk", "sus", "sux",
    "sv",  "sw",  "swb", "swc", "syc", "syr", "szl",
    "ta",  "tcy", "te",  "tem", "teo", "ter", "tet", "tg",
    "th",  "ti",  "tig", "tiv", "tk",  "tkl", "tkr", "tl",
    "tlh", "tli", "tly", "tmh", "tn",  "to",  "tog", "tpi",
    "tr",  "tru", "trv", "ts",  "tsd", "tsi", "tt",  "ttt",
    "tum", "tvl", "tw",  "twq", "ty",  "tyv", "tzm",
    "udm", "ug",  "uga", "uk",  "umb", "und", "ur",  "uz",
    "vai", "ve",  "vec", "vep", "vi",  "vls", "vmf", "vo",
    "vot", "vro", "vun",
    "wa",  "wae", "wal", "war", "was", "wbp", "wo",  "wuu",
    "xal", "xh",  "xmf", "xog",
    "yao", "yap", "yav", "ybb", "yi",  "yo",  "yrl", "yue",
    "za",  "zap", "zbl", "zea", "zen", "zgh", "zh",  "zu",
    "zun", "zxx", "zza",
NULL,
    "in",  "iw",  "ji",  "jw",  "sh",    /* obsolete language codes */
NULL
};

static const char* const DEPRECATED_LANGUAGES[]={
    "in", "iw", "ji", "jw", NULL, NULL
};
static const char* const REPLACEMENT_LANGUAGES[]={
    "id", "he", "yi", "jv", NULL, NULL
};

/**
 * Table of 3-letter language codes.
 *
 * This is a lookup table used to convert 3-letter language codes to
 * their 2-letter equivalent, where possible.  It must be kept in sync
 * with LANGUAGES.  For all valid i, LANGUAGES[i] must refer to the
 * same language as LANGUAGES_3[i].  The commented-out lines are
 * copied from LANGUAGES to make eyeballing this baby easier.
 *
 * Where a 3-letter language code has no 2-letter equivalent, the
 * 3-letter code occupies both LANGUAGES[i] and LANGUAGES_3[i].
 *
 * This table should be terminated with a NULL entry, followed by a
 * second list, and another NULL entry.  The two lists correspond to
 * the two lists in LANGUAGES.
 */
/* Generated using org.unicode.cldr.icu.GenerateISO639LanguageTables */
/* ISO639 table version is 20150505 */
static const char * const LANGUAGES_3[] = {
    "aar", "abk", "ace", "ach", "ada", "ady", "ave", "aeb",
    "afr", "afh", "agq", "ain", "aka", "akk", "akz", "ale",
    "aln", "alt", "amh", "arg", "ang", "anp", "ara", "arc",
    "arn", "aro", "arp", "arq", "arw", "ary", "arz", "asm",
    "asa", "ase", "ast", "ava", "avk", "awa", "aym", "aze",
    "bak", "bal", "ban", "bar", "bas", "bax", "bbc", "bbj",
    "bel", "bej", "bem", "bew", "bez", "bfd", "bfq", "bul",
    "bgn", "bho", "bis", "bik", "bin", "bjn", "bkm", "bla",
    "bam", "ben", "bod", "bpy", "bqi", "bre", "bra", "brh",
    "brx", "bos", "bss", "bua", "bug", "bum", "byn", "byv",
    "cat", "cad", "car", "cay", "cch", "che", "ceb", "cgg",
    "cha", "chb", "chg", "chk", "chm", "chn", "cho", "chp",
    "chr", "chy", "ckb", "cos", "cop", "cps", "cre", "crh",
    "ces", "csb", "chu", "chv", "cym",
    "dan", "dak", "dar", "dav", "deu", "del", "den", "dgr",
    "din", "dje", "doi", "dsb", "dtp", "dua", "dum", "div",
    "dyo", "dyu", "dzo", "dzg",
    "ebu", "ewe", "efi", "egl", "egy", "eka", "ell", "elx",
    "eng", "enm", "epo", "spa", "esu", "est", "eus", "ewo",
    "ext",
    "fas", "fan", "fat", "ful", "fin", "fil", "fit", "fij",
    "fao", "fon", "fra", "frc", "frm", "fro", "frp", "frr",
    "frs", "fur", "fry",
    "gle", "gaa", "gag", "gan", "gay", "gba", "gbz", "gla",
    "gez", "gil", "glg", "glk", "gmh", "grn", "goh", "gom",
    "gon", "gor", "got", "grb", "grc", "gsw", "guj", "guc",
    "gur", "guz", "glv", "gwi",
    "hau", "hai", "hak", "haw", "heb", "hin", "hif", "hil",
    "hit", "hmn", "hmo", "hrv", "hsb", "hsn", "hat", "hun",
    "hup", "hye", "her",
    "ina", "iba", "ibb", "ind", "ile", "ibo", "iii", "ipk",
    "ilo", "inh", "ido", "isl", "ita", "iku", "izh",
    "jpn", "jam", "jbo", "jgo", "jmc", "jpr", "jrb", "jut",
    "jav",
    "kat", "kaa", "kab", "kac", "kaj", "kam", "kaw", "kbd",
    "kbl", "kcg", "kde", "kea", "ken", "kfo", "kon", "kgp",
    "kha", "kho", "khq", "khw", "kik", "kiu", "kua", "kaz",
    "kkj", "kal", "kln", "khm", "kmb", "kan", "kor", "koi",
    "kok", "kos", "kpe", "kau", "krc", "kri", "krj", "krl",
    "kru", "kas", "ksb", "ksf", "ksh", "kur", "kum", "kut",
    "kom", "cor", "kir",
    "lat", "lad", "lag", "lah", "lam", "ltz", "lez", "lfn",
    "lug", "lim", "lij", "liv", "lkt", "lmo", "lin", "lao",
    "lol", "loz", "lrc", "lit", "ltg", "lub", "lua", "lui",
    "lun", "luo", "lus", "luy", "lav", "lzh", "lzz",
    "mad", "maf", "mag", "mai", "mak", "man", "mas", "mde",
    "mdf", "mdh", "mdr", "men", "mer", "mfe", "mlg", "mga",
    "mgh", "mgo", "mah", "mri", "mic", "min", "mis", "mkd",
    "mal", "mon", "mnc", "mni", "moh", "mos", "mar", "mrj",
    "msa", "mlt", "mua", "mul", "mus", "mwl", "mwr", "mwv",
    "mya", "mye", "myv", "mzn",
    "nau", "nan", "nap", "naq", "nob", "nde", "nds", "nep",
    "new", "ndo", "nia", "niu", "njo", "nld", "nmg", "nno",
    "nnh", "nor", "nog", "non", "nov", "nqo", "nbl", "nso",
    "nus", "nav", "nwc", "nya", "nym", "nyn", "nyo", "nzi",
    "oci", "oji", "orm", "ori", "oss", "osa", "ota",
    "pan", "pag", "pal", "pam", "pap", "pau", "pcd", "pdc",
    "pdt", "peo", "pfl", "phn", "pli", "pol", "pms", "pnt",
    "pon", "prg", "pro", "pus", "por",
    "que", "quc", "qug",
    "raj", "rap", "rar", "rgn", "rif", "roh", "run", "ron",
    "rof", "rom", "rtm", "rus", "rue", "rug", "rup",
    "kin", "rwk",
    "san", "sad", "sah", "sam", "saq", "sas", "sat", "saz",
    "sba", "sbp", "srd", "scn", "sco", "snd", "sdc", "sdh",
    "sme", "see", "seh", "sei", "sel", "ses", "sag", "sga",
    "sgs", "shi", "shn", "shu", "sin", "sid", "slk",
    "slv", "sli", "sly", "smo", "sma", "smj", "smn", "sms",
    "sna", "snk", "som", "sog", "sqi", "srp", "srn", "srr",
    "ssw", "ssy", "sot", "stq", "sun", "suk", "sus", "sux",
    "swe", "swa", "swb", "swc", "syc", "syr", "szl",
    "tam", "tcy", "tel", "tem", "teo", "ter", "tet", "tgk",
    "tha", "tir", "tig", "tiv", "tuk", "tkl", "tkr", "tgl",
    "tlh", "tli", "tly", "tmh", "tsn", "ton", "tog", "tpi",
    "tur", "tru", "trv", "tso", "tsd", "tsi", "tat", "ttt",
    "tum", "tvl", "twi", "twq", "tah", "tyv", "tzm",
    "udm", "uig", "uga", "ukr", "umb", "und", "urd", "uzb",
    "vai", "ven", "vec", "vep", "vie", "vls", "vmf", "vol",
    "vot", "vro", "vun",
    "wln", "wae", "wal", "war", "was", "wbp", "wol", "wuu",
    "xal", "xho", "xmf", "xog",
    "yao", "yap", "yav", "ybb", "yid", "yor", "yrl", "yue",
    "zha", "zap", "zbl", "zea", "zen", "zgh", "zho", "zul",
    "zun", "zxx", "zza",
NULL,
/*  "in",  "iw",  "ji",  "jw",  "sh",                          */
    "ind", "heb", "yid", "jaw", "srp",
NULL
};

/**
 * Table of 2-letter country codes.
 *
 * This list must be in sorted order.  This list is returned directly
 * to the user by some API.
 *
 * This list must be kept in sync with COUNTRIES_3, with corresponding
 * entries matched.
 *
 * This table should be terminated with a NULL entry, followed by a
 * second list, and another NULL entry.  The first list is visible to
 * user code when this array is returned by API.  The second list
 * contains codes we support, but do not expose through user API.
 *
 * Notes:
 *
 * ZR(ZAR) is now CD(COD) and FX(FXX) is PS(PSE) as per
 * http://www.evertype.com/standards/iso3166/iso3166-1-en.html added
 * new codes keeping the old ones for compatibility updated to include
 * 1999/12/03 revisions *CWB*
 *
 * RO(ROM) is now RO(ROU) according to
 * http://www.iso.org/iso/en/prods-services/iso3166ma/03updates-on-iso-3166/nlv3e-rou.html
 */
static const char * const COUNTRIES[] = {
    "AD",  "AE",  "AF",  "AG",  "AI",  "AL",  "AM",
    "AO",  "AQ",  "AR",  "AS",  "AT",  "AU",  "AW",  "AX",  "AZ",
    "BA",  "BB",  "BD",  "BE",  "BF",  "BG",  "BH",  "BI",
    "BJ",  "BL",  "BM",  "BN",  "BO",  "BQ",  "BR",  "BS",  "BT",  "BV",
    "BW",  "BY",  "BZ",  "CA",  "CC",  "CD",  "CF",  "CG",
    "CH",  "CI",  "CK",  "CL",  "CM",  "CN",  "CO",  "CR",
    "CU",  "CV",  "CW",  "CX",  "CY",  "CZ",  "DE",  "DJ",  "DK",
    "DM",  "DO",  "DZ",  "EC",  "EE",  "EG",  "EH",  "ER",
    "ES",  "ET",  "FI",  "FJ",  "FK",  "FM",  "FO",  "FR",
    "GA",  "GB",  "GD",  "GE",  "GF",  "GG",  "GH",  "GI",  "GL",
    "GM",  "GN",  "GP",  "GQ",  "GR",  "GS",  "GT",  "GU",
    "GW",  "GY",  "HK",  "HM",  "HN",  "HR",  "HT",  "HU",
    "ID",  "IE",  "IL",  "IM",  "IN",  "IO",  "IQ",  "IR",  "IS",
    "IT",  "JE",  "JM",  "JO",  "JP",  "KE",  "KG",  "KH",  "KI",
    "KM",  "KN",  "KP",  "KR",  "KW",  "KY",  "KZ",  "LA",
    "LB",  "LC",  "LI",  "LK",  "LR",  "LS",  "LT",  "LU",
    "LV",  "LY",  "MA",  "MC",  "MD",  "ME",  "MF",  "MG",  "MH",  "MK",
    "ML",  "MM",  "MN",  "MO",  "MP",  "MQ",  "MR",  "MS",
    "MT",  "MU",  "MV",  "MW",  "MX",  "MY",  "MZ",  "NA",
    "NC",  "NE",  "NF",  "NG",  "NI",  "NL",  "NO",  "NP",
    "NR",  "NU",  "NZ",  "OM",  "PA",  "PE",  "PF",  "PG",
    "PH",  "PK",  "PL",  "PM",  "PN",  "PR",  "PS",  "PT",
    "PW",  "PY",  "QA",  "RE",  "RO",  "RS",  "RU",  "RW",  "SA",
    "SB",  "SC",  "SD",  "SE",  "SG",  "SH",  "SI",  "SJ",
    "SK",  "SL",  "SM",  "SN",  "SO",  "SR",  "SS",  "ST",  "SV",
    "SX",  "SY",  "SZ",  "TC",  "TD",  "TF",  "TG",  "TH",  "TJ",
    "TK",  "TL",  "TM",  "TN",  "TO",  "TR",  "TT",  "TV",
    "TW",  "TZ",  "UA",  "UG",  "UM",  "US",  "UY",  "UZ",
    "VA",  "VC",  "VE",  "VG",  "VI",  "VN",  "VU",  "WF",
    "WS",  "YE",  "YT",  "ZA",  "ZM",  "ZW",
NULL,
    "AN",  "BU", "CS", "FX", "RO", "SU", "TP", "YD", "YU", "ZR",   /* obsolete country codes */
NULL
};

static const char* const DEPRECATED_COUNTRIES[] = {
    "AN", "BU", "CS", "DD", "DY", "FX", "HV", "NH", "RH", "SU", "TP", "UK", "VD", "YD", "YU", "ZR", NULL, NULL /* deprecated country list */
};
static const char* const REPLACEMENT_COUNTRIES[] = {
/*  "AN", "BU", "CS", "DD", "DY", "FX", "HV", "NH", "RH", "SU", "TP", "UK", "VD", "YD", "YU", "ZR" */
    "CW", "MM", "RS", "DE", "BJ", "FR", "BF", "VU", "ZW", "RU", "TL", "GB", "VN", "YE", "RS", "CD", NULL, NULL  /* replacement country codes */
};

/**
 * Table of 3-letter country codes.
 *
 * This is a lookup table used to convert 3-letter country codes to
 * their 2-letter equivalent.  It must be kept in sync with COUNTRIES.
 * For all valid i, COUNTRIES[i] must refer to the same country as
 * COUNTRIES_3[i].  The commented-out lines are copied from COUNTRIES
 * to make eyeballing this baby easier.
 *
 * This table should be terminated with a NULL entry, followed by a
 * second list, and another NULL entry.  The two lists correspond to
 * the two lists in COUNTRIES.
 */
static const char * const COUNTRIES_3[] = {
/*  "AD",  "AE",  "AF",  "AG",  "AI",  "AL",  "AM",      */
    "AND", "ARE", "AFG", "ATG", "AIA", "ALB", "ARM",
/*  "AO",  "AQ",  "AR",  "AS",  "AT",  "AU",  "AW",  "AX",  "AZ",     */
    "AGO", "ATA", "ARG", "ASM", "AUT", "AUS", "ABW", "ALA", "AZE",
/*  "BA",  "BB",  "BD",  "BE",  "BF",  "BG",  "BH",  "BI",     */
    "BIH", "BRB", "BGD", "BEL", "BFA", "BGR", "BHR", "BDI",
/*  "BJ",  "BL",  "BM",  "BN",  "BO",  "BQ",  "BR",  "BS",  "BT",  "BV",     */
    "BEN", "BLM", "BMU", "BRN", "BOL", "BES", "BRA", "BHS", "BTN", "BVT",
/*  "BW",  "BY",  "BZ",  "CA",  "CC",  "CD",  "CF",  "CG",     */
    "BWA", "BLR", "BLZ", "CAN", "CCK", "COD", "CAF", "COG",
/*  "CH",  "CI",  "CK",  "CL",  "CM",  "CN",  "CO",  "CR",     */
    "CHE", "CIV", "COK", "CHL", "CMR", "CHN", "COL", "CRI",
/*  "CU",  "CV",  "CW",  "CX",  "CY",  "CZ",  "DE",  "DJ",  "DK",     */
    "CUB", "CPV", "CUW", "CXR", "CYP", "CZE", "DEU", "DJI", "DNK",
/*  "DM",  "DO",  "DZ",  "EC",  "EE",  "EG",  "EH",  "ER",     */
    "DMA", "DOM", "DZA", "ECU", "EST", "EGY", "ESH", "ERI",
/*  "ES",  "ET",  "FI",  "FJ",  "FK",  "FM",  "FO",  "FR",     */
    "ESP", "ETH", "FIN", "FJI", "FLK", "FSM", "FRO", "FRA",
/*  "GA",  "GB",  "GD",  "GE",  "GF",  "GG",  "GH",  "GI",  "GL",     */
    "GAB", "GBR", "GRD", "GEO", "GUF", "GGY", "GHA", "GIB", "GRL",
/*  "GM",  "GN",  "GP",  "GQ",  "GR",  "GS",  "GT",  "GU",     */
    "GMB", "GIN", "GLP", "GNQ", "GRC", "SGS", "GTM", "GUM",
/*  "GW",  "GY",  "HK",  "HM",  "HN",  "HR",  "HT",  "HU",     */
    "GNB", "GUY", "HKG", "HMD", "HND", "HRV", "HTI", "HUN",
/*  "ID",  "IE",  "IL",  "IM",  "IN",  "IO",  "IQ",  "IR",  "IS" */
    "IDN", "IRL", "ISR", "IMN", "IND", "IOT", "IRQ", "IRN", "ISL",
/*  "IT",  "JE",  "JM",  "JO",  "JP",  "KE",  "KG",  "KH",  "KI",     */
    "ITA", "JEY", "JAM", "JOR", "JPN", "KEN", "KGZ", "KHM", "KIR",
/*  "KM",  "KN",  "KP",  "KR",  "KW",  "KY",  "KZ",  "LA",     */
    "COM", "KNA", "PRK", "KOR", "KWT", "CYM", "KAZ", "LAO",
/*  "LB",  "LC",  "LI",  "LK",  "LR",  "LS",  "LT",  "LU",     */
    "LBN", "LCA", "LIE", "LKA", "LBR", "LSO", "LTU", "LUX",
/*  "LV",  "LY",  "MA",  "MC",  "MD",  "ME",  "MF",  "MG",  "MH",  "MK",     */
    "LVA", "LBY", "MAR", "MCO", "MDA", "MNE", "MAF", "MDG", "MHL", "MKD",
/*  "ML",  "MM",  "MN",  "MO",  "MP",  "MQ",  "MR",  "MS",     */
    "MLI", "MMR", "MNG", "MAC", "MNP", "MTQ", "MRT", "MSR",
/*  "MT",  "MU",  "MV",  "MW",  "MX",  "MY",  "MZ",  "NA",     */
    "MLT", "MUS", "MDV", "MWI", "MEX", "MYS", "MOZ", "NAM",
/*  "NC",  "NE",  "NF",  "NG",  "NI",  "NL",  "NO",  "NP",     */
    "NCL", "NER", "NFK", "NGA", "NIC", "NLD", "NOR", "NPL",
/*  "NR",  "NU",  "NZ",  "OM",  "PA",  "PE",  "PF",  "PG",     */
    "NRU", "NIU", "NZL", "OMN", "PAN", "PER", "PYF", "PNG",
/*  "PH",  "PK",  "PL",  "PM",  "PN",  "PR",  "PS",  "PT",     */
    "PHL", "PAK", "POL", "SPM", "PCN", "PRI", "PSE", "PRT",
/*  "PW",  "PY",  "QA",  "RE",  "RO",  "RS",  "RU",  "RW",  "SA",     */
    "PLW", "PRY", "QAT", "REU", "ROU", "SRB", "RUS", "RWA", "SAU",
/*  "SB",  "SC",  "SD",  "SE",  "SG",  "SH",  "SI",  "SJ",     */
    "SLB", "SYC", "SDN", "SWE", "SGP", "SHN", "SVN", "SJM",
/*  "SK",  "SL",  "SM",  "SN",  "SO",  "SR",  "SS",  "ST",  "SV",     */
    "SVK", "SLE", "SMR", "SEN", "SOM", "SUR", "SSD", "STP", "SLV",
/*  "SX",  "SY",  "SZ",  "TC",  "TD",  "TF",  "TG",  "TH",  "TJ",     */
    "SXM", "SYR", "SWZ", "TCA", "TCD", "ATF", "TGO", "THA", "TJK",
/*  "TK",  "TL",  "TM",  "TN",  "TO",  "TR",  "TT",  "TV",     */
    "TKL", "TLS", "TKM", "TUN", "TON", "TUR", "TTO", "TUV",
/*  "TW",  "TZ",  "UA",  "UG",  "UM",  "US",  "UY",  "UZ",     */
    "TWN", "TZA", "UKR", "UGA", "UMI", "USA", "URY", "UZB",
/*  "VA",  "VC",  "VE",  "VG",  "VI",  "VN",  "VU",  "WF",     */
    "VAT", "VCT", "VEN", "VGB", "VIR", "VNM", "VUT", "WLF",
/*  "WS",  "YE",  "YT",  "ZA",  "ZM",  "ZW",          */
    "WSM", "YEM", "MYT", "ZAF", "ZMB", "ZWE",
NULL,
/*  "AN",  "BU",  "CS",  "FX",  "RO", "SU",  "TP",  "YD",  "YU",  "ZR" */
    "ANT", "BUR", "SCG", "FXX", "ROM", "SUN", "TMP", "YMD", "YUG", "ZAR",
NULL
};

typedef struct CanonicalizationMap {
    const char *id;          /* input ID */
    const char *canonicalID; /* canonicalized output ID */
    const char *keyword;     /* keyword, or NULL if none */
    const char *value;       /* keyword value, or NULL if kw==NULL */
} CanonicalizationMap;

/**
 * A map to canonicalize locale IDs.  This handles a variety of
 * different semantic kinds of transformations.
 */
static const CanonicalizationMap CANONICALIZE_MAP[] = {
    { "",               "en_US_POSIX", NULL, NULL }, /* .NET name */
    { "c",              "en_US_POSIX", NULL, NULL }, /* POSIX name */
    { "posix",          "en_US_POSIX", NULL, NULL }, /* POSIX name (alias of C) */
    { "art_LOJBAN",     "jbo", NULL, NULL }, /* registered name */
    { "az_AZ_CYRL",     "az_Cyrl_AZ", NULL, NULL }, /* .NET name */
    { "az_AZ_LATN",     "az_Latn_AZ", NULL, NULL }, /* .NET name */
    { "ca_ES_PREEURO",  "ca_ES", "currency", "ESP" },
    { "de__PHONEBOOK",  "de", "collation", "phonebook" }, /* Old ICU name */
    { "de_AT_PREEURO",  "de_AT", "currency", "ATS" },
    { "de_DE_PREEURO",  "de_DE", "currency", "DEM" },
    { "de_LU_PREEURO",  "de_LU", "currency", "LUF" },
    { "el_GR_PREEURO",  "el_GR", "currency", "GRD" },
    { "en_BE_PREEURO",  "en_BE", "currency", "BEF" },
    { "en_IE_PREEURO",  "en_IE", "currency", "IEP" },
    { "es__TRADITIONAL", "es", "collation", "traditional" }, /* Old ICU name */
    { "es_ES_PREEURO",  "es_ES", "currency", "ESP" },
    { "eu_ES_PREEURO",  "eu_ES", "currency", "ESP" },
    { "fi_FI_PREEURO",  "fi_FI", "currency", "FIM" },
    { "fr_BE_PREEURO",  "fr_BE", "currency", "BEF" },
    { "fr_FR_PREEURO",  "fr_FR", "currency", "FRF" },
    { "fr_LU_PREEURO",  "fr_LU", "currency", "LUF" },
    { "ga_IE_PREEURO",  "ga_IE", "currency", "IEP" },
    { "gl_ES_PREEURO",  "gl_ES", "currency", "ESP" },
    { "hi__DIRECT",     "hi", "collation", "direct" }, /* Old ICU name */
    { "it_IT_PREEURO",  "it_IT", "currency", "ITL" },
    { "ja_JP_TRADITIONAL", "ja_JP", "calendar", "japanese" }, /* Old ICU name */
    { "nb_NO_NY",       "nn_NO", NULL, NULL },  /* "markus said this was ok" :-) */
    { "nl_BE_PREEURO",  "nl_BE", "currency", "BEF" },
    { "nl_NL_PREEURO",  "nl_NL", "currency", "NLG" },
    { "pt_PT_PREEURO",  "pt_PT", "currency", "PTE" },
    { "sr_SP_CYRL",     "sr_Cyrl_RS", NULL, NULL }, /* .NET name */
    { "sr_SP_LATN",     "sr_Latn_RS", NULL, NULL }, /* .NET name */
    { "sr_YU_CYRILLIC", "sr_Cyrl_RS", NULL, NULL }, /* Linux name */
    { "th_TH_TRADITIONAL", "th_TH", "calendar", "buddhist" }, /* Old ICU name */
    { "uz_UZ_CYRILLIC", "uz_Cyrl_UZ", NULL, NULL }, /* Linux name */
    { "uz_UZ_CYRL",     "uz_Cyrl_UZ", NULL, NULL }, /* .NET name */
    { "uz_UZ_LATN",     "uz_Latn_UZ", NULL, NULL }, /* .NET name */
    { "zh_CHS",         "zh_Hans", NULL, NULL }, /* .NET name */
    { "zh_CHT",         "zh_Hant", NULL, NULL }, /* .NET name */
    { "zh_GAN",         "gan", NULL, NULL }, /* registered name */
    { "zh_GUOYU",       "zh", NULL, NULL }, /* registered name */
    { "zh_HAKKA",       "hak", NULL, NULL }, /* registered name */
    { "zh_MIN_NAN",     "nan", NULL, NULL }, /* registered name */
    { "zh_WUU",         "wuu", NULL, NULL }, /* registered name */
    { "zh_XIANG",       "hsn", NULL, NULL }, /* registered name */
    { "zh_YUE",         "yue", NULL, NULL }, /* registered name */
};

typedef struct VariantMap {
    const char *variant;          /* input ID */
    const char *keyword;     /* keyword, or NULL if none */
    const char *value;       /* keyword value, or NULL if kw==NULL */
} VariantMap;

static const VariantMap VARIANT_MAP[] = {
    { "EURO",   "currency", "EUR" },
    { "PINYIN", "collation", "pinyin" }, /* Solaris variant */
    { "STROKE", "collation", "stroke" }  /* Solaris variant */
};

/* ### BCP47 Conversion *******************************************/
/* Test if the locale id has BCP47 u extension and does not have '@' */
#define _hasBCP47Extension(id) (id && uprv_strstr(id, "@") == NULL && getShortestSubtagLength(localeID) == 1)
/* Converts the BCP47 id to Unicode id. Does nothing to id if conversion fails */
#define _ConvertBCP47(finalID, id, buffer, length,err) \
        if (uloc_forLanguageTag(id, buffer, length, NULL, err) <= 0 || U_FAILURE(*err)) { \
            finalID=id; \
        } else { \
            finalID=buffer; \
        }
/* Gets the size of the shortest subtag in the given localeID. */
static int32_t getShortestSubtagLength(const char *localeID) {
    int32_t localeIDLength = uprv_strlen(localeID);
    int32_t length = localeIDLength;
    int32_t tmpLength = 0;
    int32_t i;
    UBool reset = TRUE;

    for (i = 0; i < localeIDLength; i++) {
        if (localeID[i] != '_' && localeID[i] != '-') {
            if (reset) {
                tmpLength = 0;
                reset = FALSE;
            }
            tmpLength++;
        } else {
            if (tmpLength != 0 && tmpLength < length) {
                length = tmpLength;
            }
            reset = TRUE;
        }
    }

    return length;
}

/* ### Keywords **************************************************/

#define ULOC_KEYWORD_BUFFER_LEN 25
#define ULOC_MAX_NO_KEYWORDS 25

U_CAPI const char * U_EXPORT2
locale_getKeywordsStart(const char *localeID) {
    const char *result = NULL;
    if((result = uprv_strchr(localeID, '@')) != NULL) {
        return result;
    }
#if (U_CHARSET_FAMILY == U_EBCDIC_FAMILY)
    else {
        /* We do this because the @ sign is variant, and the @ sign used on one
        EBCDIC machine won't be compiled the same way on other EBCDIC based
        machines. */
        static const uint8_t ebcdicSigns[] = { 0x7C, 0x44, 0x66, 0x80, 0xAC, 0xAE, 0xAF, 0xB5, 0xEC, 0xEF, 0x00 };
        const uint8_t *charToFind = ebcdicSigns;
        while(*charToFind) {
            if((result = uprv_strchr(localeID, *charToFind)) != NULL) {
                return result;
            }
            charToFind++;
        }
    }
#endif
    return NULL;
}

/**
 * @param buf buffer of size [ULOC_KEYWORD_BUFFER_LEN]
 * @param keywordName incoming name to be canonicalized
 * @param status return status (keyword too long)
 * @return length of the keyword name
 */
static int32_t locale_canonKeywordName(char *buf, const char *keywordName, UErrorCode *status)
{
  int32_t i;
  int32_t keywordNameLen = (int32_t)uprv_strlen(keywordName);

  if(keywordNameLen >= ULOC_KEYWORD_BUFFER_LEN) {
    /* keyword name too long for internal buffer */
    *status = U_INTERNAL_PROGRAM_ERROR;
          return 0;
  }

  /* normalize the keyword name */
  for(i = 0; i < keywordNameLen; i++) {
    buf[i] = uprv_tolower(keywordName[i]);
  }
  buf[i] = 0;

  return keywordNameLen;
}

typedef struct {
    char keyword[ULOC_KEYWORD_BUFFER_LEN];
    int32_t keywordLen;
    const char *valueStart;
    int32_t valueLen;
} KeywordStruct;

static int32_t U_CALLCONV
compareKeywordStructs(const void * /*context*/, const void *left, const void *right) {
    const char* leftString = ((const KeywordStruct *)left)->keyword;
    const char* rightString = ((const KeywordStruct *)right)->keyword;
    return uprv_strcmp(leftString, rightString);
}

/**
 * Both addKeyword and addValue must already be in canonical form.
 * Either both addKeyword and addValue are NULL, or neither is NULL.
 * If they are not NULL they must be zero terminated.
 * If addKeyword is not NULL is must have length small enough to fit in KeywordStruct.keyword.
 */
static int32_t
_getKeywords(const char *localeID,
             char prev,
             char *keywords, int32_t keywordCapacity,
             char *values, int32_t valuesCapacity, int32_t *valLen,
             UBool valuesToo,
             const char* addKeyword,
             const char* addValue,
             UErrorCode *status)
{
    KeywordStruct keywordList[ULOC_MAX_NO_KEYWORDS];

    int32_t maxKeywords = ULOC_MAX_NO_KEYWORDS;
    int32_t numKeywords = 0;
    const char* pos = localeID;
    const char* equalSign = NULL;
    const char* semicolon = NULL;
    int32_t i = 0, j, n;
    int32_t keywordsLen = 0;
    int32_t valuesLen = 0;

    if(prev == '@') { /* start of keyword definition */
        /* we will grab pairs, trim spaces, lowercase keywords, sort and return */
        do {
            UBool duplicate = FALSE;
            /* skip leading spaces */
            while(*pos == ' ') {
                pos++;
            }
            if (!*pos) { /* handle trailing "; " */
                break;
            }
            if(numKeywords == maxKeywords) {
                *status = U_INTERNAL_PROGRAM_ERROR;
                return 0;
            }
            equalSign = uprv_strchr(pos, '=');
            semicolon = uprv_strchr(pos, ';');
            /* lack of '=' [foo@currency] is illegal */
            /* ';' before '=' [foo@currency;collation=pinyin] is illegal */
            if(!equalSign || (semicolon && semicolon<equalSign)) {
                *status = U_INVALID_FORMAT_ERROR;
                return 0;
            }
            /* need to normalize both keyword and keyword name */
            if(equalSign - pos >= ULOC_KEYWORD_BUFFER_LEN) {
                /* keyword name too long for internal buffer */
                *status = U_INTERNAL_PROGRAM_ERROR;
                return 0;
            }
            for(i = 0, n = 0; i < equalSign - pos; ++i) {
                if (pos[i] != ' ') {
                    keywordList[numKeywords].keyword[n++] = uprv_tolower(pos[i]);
                }
            }

            /* zero-length keyword is an error. */
            if (n == 0) {
                *status = U_INVALID_FORMAT_ERROR;
                return 0;
            }

            keywordList[numKeywords].keyword[n] = 0;
            keywordList[numKeywords].keywordLen = n;
            /* now grab the value part. First we skip the '=' */
            equalSign++;
            /* then we leading spaces */
            while(*equalSign == ' ') {
                equalSign++;
            }

            /* Premature end or zero-length value */
            if (!*equalSign || equalSign == semicolon) {
                *status = U_INVALID_FORMAT_ERROR;
                return 0;
            }

            keywordList[numKeywords].valueStart = equalSign;

            pos = semicolon;
            i = 0;
            if(pos) {
                while(*(pos - i - 1) == ' ') {
                    i++;
                }
                keywordList[numKeywords].valueLen = (int32_t)(pos - equalSign - i);
                pos++;
            } else {
                i = (int32_t)uprv_strlen(equalSign);
                while(i && equalSign[i-1] == ' ') {
                    i--;
                }
                keywordList[numKeywords].valueLen = i;
            }
            /* If this is a duplicate keyword, then ignore it */
            for (j=0; j<numKeywords; ++j) {
                if (uprv_strcmp(keywordList[j].keyword, keywordList[numKeywords].keyword) == 0) {
                    duplicate = TRUE;
                    break;
                }
            }
            if (!duplicate) {
                ++numKeywords;
            }
        } while(pos);

        /* Handle addKeyword/addValue. */
        if (addKeyword != NULL) {
            UBool duplicate = FALSE;
            U_ASSERT(addValue != NULL);
            /* Search for duplicate; if found, do nothing. Explicit keyword
               overrides addKeyword. */
            for (j=0; j<numKeywords; ++j) {
                if (uprv_strcmp(keywordList[j].keyword, addKeyword) == 0) {
                    duplicate = TRUE;
                    break;
                }
            }
            if (!duplicate) {
                if (numKeywords == maxKeywords) {
                    *status = U_INTERNAL_PROGRAM_ERROR;
                    return 0;
                }
                uprv_strcpy(keywordList[numKeywords].keyword, addKeyword);
                keywordList[numKeywords].keywordLen = (int32_t)uprv_strlen(addKeyword);
                keywordList[numKeywords].valueStart = addValue;
                keywordList[numKeywords].valueLen = (int32_t)uprv_strlen(addValue);
                ++numKeywords;
            }
        } else {
            U_ASSERT(addValue == NULL);
        }

        /* now we have a list of keywords */
        /* we need to sort it */
        uprv_sortArray(keywordList, numKeywords, sizeof(KeywordStruct), compareKeywordStructs, NULL, FALSE, status);

        /* Now construct the keyword part */
        for(i = 0; i < numKeywords; i++) {
            if(keywordsLen + keywordList[i].keywordLen + 1< keywordCapacity) {
                uprv_strcpy(keywords+keywordsLen, keywordList[i].keyword);
                if(valuesToo) {
                    keywords[keywordsLen + keywordList[i].keywordLen] = '=';
                } else {
                    keywords[keywordsLen + keywordList[i].keywordLen] = 0;
                }
            }
            keywordsLen += keywordList[i].keywordLen + 1;
            if(valuesToo) {
                if(keywordsLen + keywordList[i].valueLen < keywordCapacity) {
                    uprv_strncpy(keywords+keywordsLen, keywordList[i].valueStart, keywordList[i].valueLen);
                }
                keywordsLen += keywordList[i].valueLen;

                if(i < numKeywords - 1) {
                    if(keywordsLen < keywordCapacity) {
                        keywords[keywordsLen] = ';';
                    }
                    keywordsLen++;
                }
            }
            if(values) {
                if(valuesLen + keywordList[i].valueLen + 1< valuesCapacity) {
                    uprv_strcpy(values+valuesLen, keywordList[i].valueStart);
                    values[valuesLen + keywordList[i].valueLen] = 0;
                }
                valuesLen += keywordList[i].valueLen + 1;
            }
        }
        if(values) {
            values[valuesLen] = 0;
            if(valLen) {
                *valLen = valuesLen;
            }
        }
        return u_terminateChars(keywords, keywordCapacity, keywordsLen, status);
    } else {
        return 0;
    }
}

U_CFUNC int32_t
locale_getKeywords(const char *localeID,
                   char prev,
                   char *keywords, int32_t keywordCapacity,
                   char *values, int32_t valuesCapacity, int32_t *valLen,
                   UBool valuesToo,
                   UErrorCode *status) {
    return _getKeywords(localeID, prev, keywords, keywordCapacity,
                        values, valuesCapacity, valLen, valuesToo,
                        NULL, NULL, status);
}

U_CAPI int32_t U_EXPORT2
uloc_getKeywordValue(const char* localeID,
                     const char* keywordName,
                     char* buffer, int32_t bufferCapacity,
                     UErrorCode* status)
{
    const char* startSearchHere = NULL;
    const char* nextSeparator = NULL;
    char keywordNameBuffer[ULOC_KEYWORD_BUFFER_LEN];
    char localeKeywordNameBuffer[ULOC_KEYWORD_BUFFER_LEN];
    int32_t i = 0;
    int32_t result = 0;

    if(status && U_SUCCESS(*status) && localeID) {
      char tempBuffer[ULOC_FULLNAME_CAPACITY];
      const char* tmpLocaleID;

      if (_hasBCP47Extension(localeID)) {
          _ConvertBCP47(tmpLocaleID, localeID, tempBuffer, sizeof(tempBuffer), status);
      } else {
          tmpLocaleID=localeID;
      }

      startSearchHere = uprv_strchr(tmpLocaleID, '@'); /* TODO: REVISIT: shouldn't this be locale_getKeywordsStart ? */
      if(startSearchHere == NULL) {
          /* no keywords, return at once */
          return 0;
      }

      locale_canonKeywordName(keywordNameBuffer, keywordName, status);
      if(U_FAILURE(*status)) {
        return 0;
      }

      /* find the first keyword */
      while(startSearchHere) {
          startSearchHere++;
          /* skip leading spaces (allowed?) */
          while(*startSearchHere == ' ') {
              startSearchHere++;
          }
          nextSeparator = uprv_strchr(startSearchHere, '=');
          /* need to normalize both keyword and keyword name */
          if(!nextSeparator) {
              break;
          }
          if(nextSeparator - startSearchHere >= ULOC_KEYWORD_BUFFER_LEN) {
              /* keyword name too long for internal buffer */
              *status = U_INTERNAL_PROGRAM_ERROR;
              return 0;
          }
          for(i = 0; i < nextSeparator - startSearchHere; i++) {
              localeKeywordNameBuffer[i] = uprv_tolower(startSearchHere[i]);
          }
          /* trim trailing spaces */
          while(startSearchHere[i-1] == ' ') {
              i--;
              U_ASSERT(i>=0);
          }
          localeKeywordNameBuffer[i] = 0;

          startSearchHere = uprv_strchr(nextSeparator, ';');

          if(uprv_strcmp(keywordNameBuffer, localeKeywordNameBuffer) == 0) {
              nextSeparator++;
              while(*nextSeparator == ' ') {
                  nextSeparator++;
              }
              /* we actually found the keyword. Copy the value */
              if(startSearchHere && startSearchHere - nextSeparator < bufferCapacity) {
                  while(*(startSearchHere-1) == ' ') {
                      startSearchHere--;
                  }
                  uprv_strncpy(buffer, nextSeparator, startSearchHere - nextSeparator);
                  result = u_terminateChars(buffer, bufferCapacity, (int32_t)(startSearchHere - nextSeparator), status);
              } else if(!startSearchHere && (int32_t)uprv_strlen(nextSeparator) < bufferCapacity) { /* last item in string */
                  i = (int32_t)uprv_strlen(nextSeparator);
                  while(nextSeparator[i - 1] == ' ') {
                      i--;
                  }
                  uprv_strncpy(buffer, nextSeparator, i);
                  result = u_terminateChars(buffer, bufferCapacity, i, status);
              } else {
                  /* give a bigger buffer, please */
                  *status = U_BUFFER_OVERFLOW_ERROR;
                  if(startSearchHere) {
                      result = (int32_t)(startSearchHere - nextSeparator);
                  } else {
                      result = (int32_t)uprv_strlen(nextSeparator);
                  }
              }
              return result;
          }
      }
    }
    return 0;
}

U_CAPI int32_t U_EXPORT2
uloc_setKeywordValue(const char* keywordName,
                     const char* keywordValue,
                     char* buffer, int32_t bufferCapacity,
                     UErrorCode* status)
{
    /* TODO: sorting. removal. */
    int32_t keywordNameLen;
    int32_t keywordValueLen;
    int32_t bufLen;
    int32_t needLen = 0;
    int32_t foundValueLen;
    int32_t keywordAtEnd = 0; /* is the keyword at the end of the string? */
    char keywordNameBuffer[ULOC_KEYWORD_BUFFER_LEN];
    char localeKeywordNameBuffer[ULOC_KEYWORD_BUFFER_LEN];
    int32_t i = 0;
    int32_t rc;
    char* nextSeparator = NULL;
    char* nextEqualsign = NULL;
    char* startSearchHere = NULL;
    char* keywordStart = NULL;
    char *insertHere = NULL;
    if(U_FAILURE(*status)) {
        return -1;
    }
    if(bufferCapacity>1) {
        bufLen = (int32_t)uprv_strlen(buffer);
    } else {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }
    if(bufferCapacity<bufLen) {
        /* The capacity is less than the length?! Is this NULL terminated? */
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }
    if(keywordValue && !*keywordValue) {
        keywordValue = NULL;
    }
    if(keywordValue) {
        keywordValueLen = (int32_t)uprv_strlen(keywordValue);
    } else {
        keywordValueLen = 0;
    }
    keywordNameLen = locale_canonKeywordName(keywordNameBuffer, keywordName, status);
    if(U_FAILURE(*status)) {
        return 0;
    }
    startSearchHere = (char*)locale_getKeywordsStart(buffer);
    if(startSearchHere == NULL || (startSearchHere[1]==0)) {
        if(!keywordValue) { /* no keywords = nothing to remove */
            return bufLen;
        }

        needLen = bufLen+1+keywordNameLen+1+keywordValueLen;
        if(startSearchHere) { /* had a single @ */
            needLen--; /* already had the @ */
            /* startSearchHere points at the @ */
        } else {
            startSearchHere=buffer+bufLen;
        }
        if(needLen >= bufferCapacity) {
            *status = U_BUFFER_OVERFLOW_ERROR;
            return needLen; /* no change */
        }
        *startSearchHere = '@';
        startSearchHere++;
        uprv_strcpy(startSearchHere, keywordNameBuffer);
        startSearchHere += keywordNameLen;
        *startSearchHere = '=';
        startSearchHere++;
        uprv_strcpy(startSearchHere, keywordValue);
        startSearchHere+=keywordValueLen;
        return needLen;
    } /* end shortcut - no @ */

    keywordStart = startSearchHere;
    /* search for keyword */
    while(keywordStart) {
        keywordStart++;
        /* skip leading spaces (allowed?) */
        while(*keywordStart == ' ') {
            keywordStart++;
        }
        nextEqualsign = uprv_strchr(keywordStart, '=');
        /* need to normalize both keyword and keyword name */
        if(!nextEqualsign) {
            break;
        }
        if(nextEqualsign - keywordStart >= ULOC_KEYWORD_BUFFER_LEN) {
            /* keyword name too long for internal buffer */
            *status = U_INTERNAL_PROGRAM_ERROR;
            return 0;
        }
        for(i = 0; i < nextEqualsign - keywordStart; i++) {
            localeKeywordNameBuffer[i] = uprv_tolower(keywordStart[i]);
        }
        /* trim trailing spaces */
        while(keywordStart[i-1] == ' ') {
            i--;
        }
        U_ASSERT(i>=0 && i<ULOC_KEYWORD_BUFFER_LEN);
        localeKeywordNameBuffer[i] = 0;

        nextSeparator = uprv_strchr(nextEqualsign, ';');
        rc = uprv_strcmp(keywordNameBuffer, localeKeywordNameBuffer);
        if(rc == 0) {
            nextEqualsign++;
            while(*nextEqualsign == ' ') {
                nextEqualsign++;
            }
            /* we actually found the keyword. Change the value */
            if (nextSeparator) {
                keywordAtEnd = 0;
                foundValueLen = (int32_t)(nextSeparator - nextEqualsign);
            } else {
                keywordAtEnd = 1;
                foundValueLen = (int32_t)uprv_strlen(nextEqualsign);
            }
            if(keywordValue) { /* adding a value - not removing */
              if(foundValueLen == keywordValueLen) {
                uprv_strncpy(nextEqualsign, keywordValue, keywordValueLen);
                return bufLen; /* no change in size */
              } else if(foundValueLen > keywordValueLen) {
                int32_t delta = foundValueLen - keywordValueLen;
                if(nextSeparator) { /* RH side */
                  uprv_memmove(nextSeparator - delta, nextSeparator, bufLen-(nextSeparator-buffer));
                }
                uprv_strncpy(nextEqualsign, keywordValue, keywordValueLen);
                bufLen -= delta;
                buffer[bufLen]=0;
                return bufLen;
              } else { /* FVL < KVL */
                int32_t delta = keywordValueLen - foundValueLen;
                if((bufLen+delta) >= bufferCapacity) {
                  *status = U_BUFFER_OVERFLOW_ERROR;
                  return bufLen+delta;
                }
                if(nextSeparator) { /* RH side */
                  uprv_memmove(nextSeparator+delta,nextSeparator, bufLen-(nextSeparator-buffer));
                }
                uprv_strncpy(nextEqualsign, keywordValue, keywordValueLen);
                bufLen += delta;
                buffer[bufLen]=0;
                return bufLen;
              }
            } else { /* removing a keyword */
              if(keywordAtEnd) {
                /* zero out the ';' or '@' just before startSearchhere */
                keywordStart[-1] = 0;
                return (int32_t)((keywordStart-buffer)-1); /* (string length without keyword) minus separator */
              } else {
                uprv_memmove(keywordStart, nextSeparator+1, bufLen-((nextSeparator+1)-buffer));
                keywordStart[bufLen-((nextSeparator+1)-buffer)]=0;
                return (int32_t)(bufLen-((nextSeparator+1)-keywordStart));
              }
            }
        } else if(rc<0){ /* end match keyword */
          /* could insert at this location. */
          insertHere = keywordStart;
        }
        keywordStart = nextSeparator;
    } /* end loop searching */

    if(!keywordValue) {
      return bufLen; /* removal of non-extant keyword - no change */
    }

    /* we know there is at least one keyword. */
    needLen = bufLen+1+keywordNameLen+1+keywordValueLen;
    if(needLen >= bufferCapacity) {
        *status = U_BUFFER_OVERFLOW_ERROR;
        return needLen; /* no change */
    }

    if(insertHere) {
      uprv_memmove(insertHere+(1+keywordNameLen+1+keywordValueLen), insertHere, bufLen-(insertHere-buffer));
      keywordStart = insertHere;
    } else {
      keywordStart = buffer+bufLen;
      *keywordStart = ';';
      keywordStart++;
    }
    uprv_strncpy(keywordStart, keywordNameBuffer, keywordNameLen);
    keywordStart += keywordNameLen;
    *keywordStart = '=';
    keywordStart++;
    uprv_strncpy(keywordStart, keywordValue, keywordValueLen); /* terminates. */
    keywordStart+=keywordValueLen;
    if(insertHere) {
      *keywordStart = ';';
      keywordStart++;
    }
    buffer[needLen]=0;
    return needLen;
}

/* ### ID parsing implementation **************************************************/

#define _isPrefixLetter(a) ((a=='x')||(a=='X')||(a=='i')||(a=='I'))

/*returns TRUE if one of the special prefixes is here (s=string)
  'x-' or 'i-' */
#define _isIDPrefix(s) (_isPrefixLetter(s[0])&&_isIDSeparator(s[1]))

/* Dot terminates it because of POSIX form  where dot precedes the codepage
 * except for variant
 */
#define _isTerminator(a)  ((a==0)||(a=='.')||(a=='@'))

static char* _strnchr(const char* str, int32_t len, char c) {
    U_ASSERT(str != 0 && len >= 0);
    while (len-- != 0) {
        char d = *str;
        if (d == c) {
            return (char*) str;
        } else if (d == 0) {
            break;
        }
        ++str;
    }
    return NULL;
}

/**
 * Lookup 'key' in the array 'list'.  The array 'list' should contain
 * a NULL entry, followed by more entries, and a second NULL entry.
 *
 * The 'list' param should be LANGUAGES, LANGUAGES_3, COUNTRIES, or
 * COUNTRIES_3.
 */
static int16_t _findIndex(const char* const* list, const char* key)
{
    const char* const* anchor = list;
    int32_t pass = 0;

    /* Make two passes through two NULL-terminated arrays at 'list' */
    while (pass++ < 2) {
        while (*list) {
            if (uprv_strcmp(key, *list) == 0) {
                return (int16_t)(list - anchor);
            }
            list++;
        }
        ++list;     /* skip final NULL *CWB*/
    }
    return -1;
}

/* count the length of src while copying it to dest; return strlen(src) */
static inline int32_t
_copyCount(char *dest, int32_t destCapacity, const char *src) {
    const char *anchor;
    char c;

    anchor=src;
    for(;;) {
        if((c=*src)==0) {
            return (int32_t)(src-anchor);
        }
        if(destCapacity<=0) {
            return (int32_t)((src-anchor)+uprv_strlen(src));
        }
        ++src;
        *dest++=c;
        --destCapacity;
    }
}

U_CFUNC const char*
uloc_getCurrentCountryID(const char* oldID){
    int32_t offset = _findIndex(DEPRECATED_COUNTRIES, oldID);
    if (offset >= 0) {
        return REPLACEMENT_COUNTRIES[offset];
    }
    return oldID;
}
U_CFUNC const char*
uloc_getCurrentLanguageID(const char* oldID){
    int32_t offset = _findIndex(DEPRECATED_LANGUAGES, oldID);
    if (offset >= 0) {
        return REPLACEMENT_LANGUAGES[offset];
    }
    return oldID;
}
/*
 * the internal functions _getLanguage(), _getCountry(), _getVariant()
 * avoid duplicating code to handle the earlier locale ID pieces
 * in the functions for the later ones by
 * setting the *pEnd pointer to where they stopped parsing
 *
 * TODO try to use this in Locale
 */
U_CFUNC int32_t
ulocimp_getLanguage(const char *localeID,
                    char *language, int32_t languageCapacity,
                    const char **pEnd) {
    int32_t i=0;
    int32_t offset;
    char lang[4]={ 0, 0, 0, 0 }; /* temporary buffer to hold language code for searching */

    /* if it starts with i- or x- then copy that prefix */
    if(_isIDPrefix(localeID)) {
        if(i<languageCapacity) {
            language[i]=(char)uprv_tolower(*localeID);
        }
        if(i<languageCapacity) {
            language[i+1]='-';
        }
        i+=2;
        localeID+=2;
    }

    /* copy the language as far as possible and count its length */
    while(!_isTerminator(*localeID) && !_isIDSeparator(*localeID)) {
        if(i<languageCapacity) {
            language[i]=(char)uprv_tolower(*localeID);
        }
        if(i<3) {
            U_ASSERT(i>=0);
            lang[i]=(char)uprv_tolower(*localeID);
        }
        i++;
        localeID++;
    }

    if(i==3) {
        /* convert 3 character code to 2 character code if possible *CWB*/
        offset=_findIndex(LANGUAGES_3, lang);
        if(offset>=0) {
            i=_copyCount(language, languageCapacity, LANGUAGES[offset]);
        }
    }

    if(pEnd!=NULL) {
        *pEnd=localeID;
    }
    return i;
}

U_CFUNC int32_t
ulocimp_getScript(const char *localeID,
                  char *script, int32_t scriptCapacity,
                  const char **pEnd)
{
    int32_t idLen = 0;

    if (pEnd != NULL) {
        *pEnd = localeID;
    }

    /* copy the second item as far as possible and count its length */
    while(!_isTerminator(localeID[idLen]) && !_isIDSeparator(localeID[idLen])
            && uprv_isASCIILetter(localeID[idLen])) {
        idLen++;
    }

    /* If it's exactly 4 characters long, then it's a script and not a country. */
    if (idLen == 4) {
        int32_t i;
        if (pEnd != NULL) {
            *pEnd = localeID+idLen;
        }
        if(idLen > scriptCapacity) {
            idLen = scriptCapacity;
        }
        if (idLen >= 1) {
            script[0]=(char)uprv_toupper(*(localeID++));
        }
        for (i = 1; i < idLen; i++) {
            script[i]=(char)uprv_tolower(*(localeID++));
        }
    }
    else {
        idLen = 0;
    }
    return idLen;
}

U_CFUNC int32_t
ulocimp_getCountry(const char *localeID,
                   char *country, int32_t countryCapacity,
                   const char **pEnd)
{
    int32_t idLen=0;
    char cnty[ULOC_COUNTRY_CAPACITY]={ 0, 0, 0, 0 };
    int32_t offset;

    /* copy the country as far as possible and count its length */
    while(!_isTerminator(localeID[idLen]) && !_isIDSeparator(localeID[idLen])) {
        if(idLen<(ULOC_COUNTRY_CAPACITY-1)) {   /*CWB*/
            cnty[idLen]=(char)uprv_toupper(localeID[idLen]);
        }
        idLen++;
    }

    /* the country should be either length 2 or 3 */
    if (idLen == 2 || idLen == 3) {
        UBool gotCountry = FALSE;
        /* convert 3 character code to 2 character code if possible *CWB*/
        if(idLen==3) {
            offset=_findIndex(COUNTRIES_3, cnty);
            if(offset>=0) {
                idLen=_copyCount(country, countryCapacity, COUNTRIES[offset]);
                gotCountry = TRUE;
            }
        }
        if (!gotCountry) {
            int32_t i = 0;
            for (i = 0; i < idLen; i++) {
                if (i < countryCapacity) {
                    country[i]=(char)uprv_toupper(localeID[i]);
                }
            }
        }
        localeID+=idLen;
    } else {
        idLen = 0;
    }

    if(pEnd!=NULL) {
        *pEnd=localeID;
    }

    return idLen;
}

/**
 * @param needSeparator if true, then add leading '_' if any variants
 * are added to 'variant'
 */
static int32_t
_getVariantEx(const char *localeID,
              char prev,
              char *variant, int32_t variantCapacity,
              UBool needSeparator) {
    int32_t i=0;

    /* get one or more variant tags and separate them with '_' */
    if(_isIDSeparator(prev)) {
        /* get a variant string after a '-' or '_' */
        while(!_isTerminator(*localeID)) {
            if (needSeparator) {
                if (i<variantCapacity) {
                    variant[i] = '_';
                }
                ++i;
                needSeparator = FALSE;
            }
            if(i<variantCapacity) {
                variant[i]=(char)uprv_toupper(*localeID);
                if(variant[i]=='-') {
                    variant[i]='_';
                }
            }
            i++;
            localeID++;
        }
    }

    /* if there is no variant tag after a '-' or '_' then look for '@' */
    if(i==0) {
        if(prev=='@') {
            /* keep localeID */
        } else if((localeID=locale_getKeywordsStart(localeID))!=NULL) {
            ++localeID; /* point after the '@' */
        } else {
            return 0;
        }
        while(!_isTerminator(*localeID)) {
            if (needSeparator) {
                if (i<variantCapacity) {
                    variant[i] = '_';
                }
                ++i;
                needSeparator = FALSE;
            }
            if(i<variantCapacity) {
                variant[i]=(char)uprv_toupper(*localeID);
                if(variant[i]=='-' || variant[i]==',') {
                    variant[i]='_';
                }
            }
            i++;
            localeID++;
        }
    }

    return i;
}

static int32_t
_getVariant(const char *localeID,
            char prev,
            char *variant, int32_t variantCapacity) {
    return _getVariantEx(localeID, prev, variant, variantCapacity, FALSE);
}

/**
 * Delete ALL instances of a variant from the given list of one or
 * more variants.  Example: "FOO_EURO_BAR_EURO" => "FOO_BAR".
 * @param variants the source string of one or more variants,
 * separated by '_'.  This will be MODIFIED IN PLACE.  Not zero
 * terminated; if it is, trailing zero will NOT be maintained.
 * @param variantsLen length of variants
 * @param toDelete variant to delete, without separators, e.g.  "EURO"
 * or "PREEURO"; not zero terminated
 * @param toDeleteLen length of toDelete
 * @return number of characters deleted from variants
 */
static int32_t
_deleteVariant(char* variants, int32_t variantsLen,
               const char* toDelete, int32_t toDeleteLen)
{
    int32_t delta = 0; /* number of chars deleted */
    for (;;) {
        UBool flag = FALSE;
        if (variantsLen < toDeleteLen) {
            return delta;
        }
        if (uprv_strncmp(variants, toDelete, toDeleteLen) == 0 &&
            (variantsLen == toDeleteLen ||
             (flag=(variants[toDeleteLen] == '_'))))
        {
            int32_t d = toDeleteLen + (flag?1:0);
            variantsLen -= d;
            delta += d;
            if (variantsLen > 0) {
                uprv_memmove(variants, variants+d, variantsLen);
            }
        } else {
            char* p = _strnchr(variants, variantsLen, '_');
            if (p == NULL) {
                return delta;
            }
            ++p;
            variantsLen -= (int32_t)(p - variants);
            variants = p;
        }
    }
}

/* Keyword enumeration */

typedef struct UKeywordsContext {
    char* keywords;
    char* current;
} UKeywordsContext;

U_CDECL_BEGIN

static void U_CALLCONV
uloc_kw_closeKeywords(UEnumeration *enumerator) {
    uprv_free(((UKeywordsContext *)enumerator->context)->keywords);
    uprv_free(enumerator->context);
    uprv_free(enumerator);
}

static int32_t U_CALLCONV
uloc_kw_countKeywords(UEnumeration *en, UErrorCode * /*status*/) {
    char *kw = ((UKeywordsContext *)en->context)->keywords;
    int32_t result = 0;
    while(*kw) {
        result++;
        kw += uprv_strlen(kw)+1;
    }
    return result;
}

static const char * U_CALLCONV
uloc_kw_nextKeyword(UEnumeration* en,
                    int32_t* resultLength,
                    UErrorCode* /*status*/) {
    const char* result = ((UKeywordsContext *)en->context)->current;
    int32_t len = 0;
    if(*result) {
        len = (int32_t)uprv_strlen(((UKeywordsContext *)en->context)->current);
        ((UKeywordsContext *)en->context)->current += len+1;
    } else {
        result = NULL;
    }
    if (resultLength) {
        *resultLength = len;
    }
    return result;
}

static void U_CALLCONV
uloc_kw_resetKeywords(UEnumeration* en,
                      UErrorCode* /*status*/) {
    ((UKeywordsContext *)en->context)->current = ((UKeywordsContext *)en->context)->keywords;
}

U_CDECL_END


static const UEnumeration gKeywordsEnum = {
    NULL,
    NULL,
    uloc_kw_closeKeywords,
    uloc_kw_countKeywords,
    uenum_unextDefault,
    uloc_kw_nextKeyword,
    uloc_kw_resetKeywords
};

U_CAPI UEnumeration* U_EXPORT2
uloc_openKeywordList(const char *keywordList, int32_t keywordListSize, UErrorCode* status)
{
    UKeywordsContext *myContext = NULL;
    UEnumeration *result = NULL;

    if(U_FAILURE(*status)) {
        return NULL;
    }
    result = (UEnumeration *)uprv_malloc(sizeof(UEnumeration));
    /* Null pointer test */
    if (result == NULL) {
        *status = U_MEMORY_ALLOCATION_ERROR;
        return NULL;
    }
    uprv_memcpy(result, &gKeywordsEnum, sizeof(UEnumeration));
    myContext = static_cast<UKeywordsContext *>(uprv_malloc(sizeof(UKeywordsContext)));
    if (myContext == NULL) {
        *status = U_MEMORY_ALLOCATION_ERROR;
        uprv_free(result);
        return NULL;
    }
    myContext->keywords = (char *)uprv_malloc(keywordListSize+1);
    uprv_memcpy(myContext->keywords, keywordList, keywordListSize);
    myContext->keywords[keywordListSize] = 0;
    myContext->current = myContext->keywords;
    result->context = myContext;
    return result;
}

U_CAPI UEnumeration* U_EXPORT2
uloc_openKeywords(const char* localeID,
                        UErrorCode* status)
{
    int32_t i=0;
    char keywords[256];
    int32_t keywordsCapacity = 256;
    char tempBuffer[ULOC_FULLNAME_CAPACITY];
    const char* tmpLocaleID;

    if(status==NULL || U_FAILURE(*status)) {
        return 0;
    }

    if (_hasBCP47Extension(localeID)) {
        _ConvertBCP47(tmpLocaleID, localeID, tempBuffer, sizeof(tempBuffer), status);
    } else {
        if (localeID==NULL) {
           localeID=uloc_getDefault();
        }
        tmpLocaleID=localeID;
    }

    /* Skip the language */
    ulocimp_getLanguage(tmpLocaleID, NULL, 0, &tmpLocaleID);
    if(_isIDSeparator(*tmpLocaleID)) {
        const char *scriptID;
        /* Skip the script if available */
        ulocimp_getScript(tmpLocaleID+1, NULL, 0, &scriptID);
        if(scriptID != tmpLocaleID+1) {
            /* Found optional script */
            tmpLocaleID = scriptID;
        }
        /* Skip the Country */
        if (_isIDSeparator(*tmpLocaleID)) {
            ulocimp_getCountry(tmpLocaleID+1, NULL, 0, &tmpLocaleID);
            if(_isIDSeparator(*tmpLocaleID)) {
                _getVariant(tmpLocaleID+1, *tmpLocaleID, NULL, 0);
            }
        }
    }

    /* keywords are located after '@' */
    if((tmpLocaleID = locale_getKeywordsStart(tmpLocaleID)) != NULL) {
        i=locale_getKeywords(tmpLocaleID+1, '@', keywords, keywordsCapacity, NULL, 0, NULL, FALSE, status);
    }

    if(i) {
        return uloc_openKeywordList(keywords, i, status);
    } else {
        return NULL;
    }
}


/* bit-flags for 'options' parameter of _canonicalize */
#define _ULOC_STRIP_KEYWORDS 0x2
#define _ULOC_CANONICALIZE   0x1

#define OPTION_SET(options, mask) ((options & mask) != 0)

static const char i_default[] = {'i', '-', 'd', 'e', 'f', 'a', 'u', 'l', 't'};
#define I_DEFAULT_LENGTH UPRV_LENGTHOF(i_default)

/**
 * Canonicalize the given localeID, to level 1 or to level 2,
 * depending on the options.  To specify level 1, pass in options=0.
 * To specify level 2, pass in options=_ULOC_CANONICALIZE.
 *
 * This is the code underlying uloc_getName and uloc_canonicalize.
 */
static int32_t
_canonicalize(const char* localeID,
              char* result,
              int32_t resultCapacity,
              uint32_t options,
              UErrorCode* err) {
    int32_t j, len, fieldCount=0, scriptSize=0, variantSize=0, nameCapacity;
    char localeBuffer[ULOC_FULLNAME_CAPACITY];
    char tempBuffer[ULOC_FULLNAME_CAPACITY];
    const char* origLocaleID;
    const char* tmpLocaleID;
    const char* keywordAssign = NULL;
    const char* separatorIndicator = NULL;
    const char* addKeyword = NULL;
    const char* addValue = NULL;
    char* name;
    char* variant = NULL; /* pointer into name, or NULL */

    if (U_FAILURE(*err)) {
        return 0;
    }

    if (_hasBCP47Extension(localeID)) {
        _ConvertBCP47(tmpLocaleID, localeID, tempBuffer, sizeof(tempBuffer), err);
    } else {
        if (localeID==NULL) {
           localeID=uloc_getDefault();
        }
        tmpLocaleID=localeID;
    }

    origLocaleID=tmpLocaleID;

    /* if we are doing a full canonicalization, then put results in
       localeBuffer, if necessary; otherwise send them to result. */
    if (/*OPTION_SET(options, _ULOC_CANONICALIZE) &&*/
        (result == NULL || resultCapacity < (int32_t)sizeof(localeBuffer))) {
        name = localeBuffer;
        nameCapacity = (int32_t)sizeof(localeBuffer);
    } else {
        name = result;
        nameCapacity = resultCapacity;
    }

    /* get all pieces, one after another, and separate with '_' */
    len=ulocimp_getLanguage(tmpLocaleID, name, nameCapacity, &tmpLocaleID);

    if(len == I_DEFAULT_LENGTH && uprv_strncmp(origLocaleID, i_default, len) == 0) {
        const char *d = uloc_getDefault();

        len = (int32_t)uprv_strlen(d);

        if (name != NULL) {
            uprv_strncpy(name, d, len);
        }
    } else if(_isIDSeparator(*tmpLocaleID)) {
        const char *scriptID;

        ++fieldCount;
        if(len<nameCapacity) {
            name[len]='_';
        }
        ++len;

        scriptSize=ulocimp_getScript(tmpLocaleID+1,
            (len<nameCapacity ? name+len : NULL), nameCapacity-len, &scriptID);
        if(scriptSize > 0) {
            /* Found optional script */
            tmpLocaleID = scriptID;
            ++fieldCount;
            len+=scriptSize;
            if (_isIDSeparator(*tmpLocaleID)) {
                /* If there is something else, then we add the _ */
                if(len<nameCapacity) {
                    name[len]='_';
                }
                ++len;
            }
        }

        if (_isIDSeparator(*tmpLocaleID)) {
            const char *cntryID;
            int32_t cntrySize = ulocimp_getCountry(tmpLocaleID+1,
                (len<nameCapacity ? name+len : NULL), nameCapacity-len, &cntryID);
            if (cntrySize > 0) {
                /* Found optional country */
                tmpLocaleID = cntryID;
                len+=cntrySize;
            }
            if(_isIDSeparator(*tmpLocaleID)) {
                /* If there is something else, then we add the _  if we found country before. */
                if (cntrySize >= 0 && ! _isIDSeparator(*(tmpLocaleID+1)) ) {
                    ++fieldCount;
                    if(len<nameCapacity) {
                        name[len]='_';
                    }
                    ++len;
                }

                variantSize = _getVariant(tmpLocaleID+1, *tmpLocaleID,
                    (len<nameCapacity ? name+len : NULL), nameCapacity-len);
                if (variantSize > 0) {
                    variant = len<nameCapacity ? name+len : NULL;
                    len += variantSize;
                    tmpLocaleID += variantSize + 1; /* skip '_' and variant */
                }
            }
        }
    }

    /* Copy POSIX-style charset specifier, if any [mr.utf8] */
    if (!OPTION_SET(options, _ULOC_CANONICALIZE) && *tmpLocaleID == '.') {
        UBool done = FALSE;
        do {
            char c = *tmpLocaleID;
            switch (c) {
            case 0:
            case '@':
                done = TRUE;
                break;
            default:
                if (len<nameCapacity) {
                    name[len] = c;
                }
                ++len;
                ++tmpLocaleID;
                break;
            }
        } while (!done);
    }

    /* Scan ahead to next '@' and determine if it is followed by '=' and/or ';'
       After this, tmpLocaleID either points to '@' or is NULL */
    if ((tmpLocaleID=locale_getKeywordsStart(tmpLocaleID))!=NULL) {
        keywordAssign = uprv_strchr(tmpLocaleID, '=');
        separatorIndicator = uprv_strchr(tmpLocaleID, ';');
    }

    /* Copy POSIX-style variant, if any [mr@FOO] */
    if (!OPTION_SET(options, _ULOC_CANONICALIZE) &&
        tmpLocaleID != NULL && keywordAssign == NULL) {
        for (;;) {
            char c = *tmpLocaleID;
            if (c == 0) {
                break;
            }
            if (len<nameCapacity) {
                name[len] = c;
            }
            ++len;
            ++tmpLocaleID;
        }
    }

    if (OPTION_SET(options, _ULOC_CANONICALIZE)) {
        /* Handle @FOO variant if @ is present and not followed by = */
        if (tmpLocaleID!=NULL && keywordAssign==NULL) {
            int32_t posixVariantSize;
            /* Add missing '_' if needed */
            if (fieldCount < 2 || (fieldCount < 3 && scriptSize > 0)) {
                do {
                    if(len<nameCapacity) {
                        name[len]='_';
                    }
                    ++len;
                    ++fieldCount;
                } while(fieldCount<2);
            }
            posixVariantSize = _getVariantEx(tmpLocaleID+1, '@', name+len, nameCapacity-len,
                                             (UBool)(variantSize > 0));
            if (posixVariantSize > 0) {
                if (variant == NULL) {
                    variant = name+len;
                }
                len += posixVariantSize;
                variantSize += posixVariantSize;
            }
        }

        /* Handle generic variants first */
        if (variant) {
            for (j=0; j<UPRV_LENGTHOF(VARIANT_MAP); j++) {
                const char* variantToCompare = VARIANT_MAP[j].variant;
                int32_t n = (int32_t)uprv_strlen(variantToCompare);
                int32_t variantLen = _deleteVariant(variant, uprv_min(variantSize, (nameCapacity-len)), variantToCompare, n);
                len -= variantLen;
                if (variantLen > 0) {
                    if (len > 0 && name[len-1] == '_') { /* delete trailing '_' */
                        --len;
                    }
                    addKeyword = VARIANT_MAP[j].keyword;
                    addValue = VARIANT_MAP[j].value;
                    break;
                }
            }
            if (len > 0 && len <= nameCapacity && name[len-1] == '_') { /* delete trailing '_' */
                --len;
            }
        }

        /* Look up the ID in the canonicalization map */
        for (j=0; j<UPRV_LENGTHOF(CANONICALIZE_MAP); j++) {
            const char* id = CANONICALIZE_MAP[j].id;
            int32_t n = (int32_t)uprv_strlen(id);
            if (len == n && uprv_strncmp(name, id, n) == 0) {
                if (n == 0 && tmpLocaleID != NULL) {
                    break; /* Don't remap "" if keywords present */
                }
                len = _copyCount(name, nameCapacity, CANONICALIZE_MAP[j].canonicalID);
                if (CANONICALIZE_MAP[j].keyword) {
                    addKeyword = CANONICALIZE_MAP[j].keyword;
                    addValue = CANONICALIZE_MAP[j].value;
                }
                break;
            }
        }
    }

    if (!OPTION_SET(options, _ULOC_STRIP_KEYWORDS)) {
        if (tmpLocaleID!=NULL && keywordAssign!=NULL &&
            (!separatorIndicator || separatorIndicator > keywordAssign)) {
            if(len<nameCapacity) {
                name[len]='@';
            }
            ++len;
            ++fieldCount;
            len += _getKeywords(tmpLocaleID+1, '@', (len<nameCapacity ? name+len : NULL), nameCapacity-len,
                                NULL, 0, NULL, TRUE, addKeyword, addValue, err);
        } else if (addKeyword != NULL) {
            U_ASSERT(addValue != NULL && len < nameCapacity);
            /* inelegant but works -- later make _getKeywords do this? */
            len += _copyCount(name+len, nameCapacity-len, "@");
            len += _copyCount(name+len, nameCapacity-len, addKeyword);
            len += _copyCount(name+len, nameCapacity-len, "=");
            len += _copyCount(name+len, nameCapacity-len, addValue);
        }
    }

    if (U_SUCCESS(*err) && result != NULL && name == localeBuffer) {
        uprv_strncpy(result, localeBuffer, (len > resultCapacity) ? resultCapacity : len);
    }

    return u_terminateChars(result, resultCapacity, len, err);
}

/* ### ID parsing API **************************************************/

U_CAPI int32_t  U_EXPORT2
uloc_getParent(const char*    localeID,
               char* parent,
               int32_t parentCapacity,
               UErrorCode* err)
{
    const char *lastUnderscore;
    int32_t i;

    if (U_FAILURE(*err))
        return 0;

    if (localeID == NULL)
        localeID = uloc_getDefault();

    lastUnderscore=uprv_strrchr(localeID, '_');
    if(lastUnderscore!=NULL) {
        i=(int32_t)(lastUnderscore-localeID);
    } else {
        i=0;
    }

    if(i>0 && parent != localeID) {
        uprv_memcpy(parent, localeID, uprv_min(i, parentCapacity));
    }
    return u_terminateChars(parent, parentCapacity, i, err);
}

U_CAPI int32_t U_EXPORT2
uloc_getLanguage(const char*    localeID,
         char* language,
         int32_t languageCapacity,
         UErrorCode* err)
{
    /* uloc_getLanguage will return a 2 character iso-639 code if one exists. *CWB*/
    int32_t i=0;

    if (err==NULL || U_FAILURE(*err)) {
        return 0;
    }

    if(localeID==NULL) {
        localeID=uloc_getDefault();
    }

    i=ulocimp_getLanguage(localeID, language, languageCapacity, NULL);
    return u_terminateChars(language, languageCapacity, i, err);
}

U_CAPI int32_t U_EXPORT2
uloc_getScript(const char*    localeID,
         char* script,
         int32_t scriptCapacity,
         UErrorCode* err)
{
    int32_t i=0;

    if(err==NULL || U_FAILURE(*err)) {
        return 0;
    }

    if(localeID==NULL) {
        localeID=uloc_getDefault();
    }

    /* skip the language */
    ulocimp_getLanguage(localeID, NULL, 0, &localeID);
    if(_isIDSeparator(*localeID)) {
        i=ulocimp_getScript(localeID+1, script, scriptCapacity, NULL);
    }
    return u_terminateChars(script, scriptCapacity, i, err);
}

U_CAPI int32_t  U_EXPORT2
uloc_getCountry(const char* localeID,
            char* country,
            int32_t countryCapacity,
            UErrorCode* err)
{
    int32_t i=0;

    if(err==NULL || U_FAILURE(*err)) {
        return 0;
    }

    if(localeID==NULL) {
        localeID=uloc_getDefault();
    }

    /* Skip the language */
    ulocimp_getLanguage(localeID, NULL, 0, &localeID);
    if(_isIDSeparator(*localeID)) {
        const char *scriptID;
        /* Skip the script if available */
        ulocimp_getScript(localeID+1, NULL, 0, &scriptID);
        if(scriptID != localeID+1) {
            /* Found optional script */
            localeID = scriptID;
        }
        if(_isIDSeparator(*localeID)) {
            i=ulocimp_getCountry(localeID+1, country, countryCapacity, NULL);
        }
    }
    return u_terminateChars(country, countryCapacity, i, err);
}

U_CAPI int32_t  U_EXPORT2
uloc_getVariant(const char* localeID,
                char* variant,
                int32_t variantCapacity,
                UErrorCode* err)
{
    char tempBuffer[ULOC_FULLNAME_CAPACITY];
    const char* tmpLocaleID;
    int32_t i=0;

    if(err==NULL || U_FAILURE(*err)) {
        return 0;
    }

    if (_hasBCP47Extension(localeID)) {
        _ConvertBCP47(tmpLocaleID, localeID, tempBuffer, sizeof(tempBuffer), err);
    } else {
        if (localeID==NULL) {
           localeID=uloc_getDefault();
        }
        tmpLocaleID=localeID;
    }

    /* Skip the language */
    ulocimp_getLanguage(tmpLocaleID, NULL, 0, &tmpLocaleID);
    if(_isIDSeparator(*tmpLocaleID)) {
        const char *scriptID;
        /* Skip the script if available */
        ulocimp_getScript(tmpLocaleID+1, NULL, 0, &scriptID);
        if(scriptID != tmpLocaleID+1) {
            /* Found optional script */
            tmpLocaleID = scriptID;
        }
        /* Skip the Country */
        if (_isIDSeparator(*tmpLocaleID)) {
            const char *cntryID;
            ulocimp_getCountry(tmpLocaleID+1, NULL, 0, &cntryID);
            if (cntryID != tmpLocaleID+1) {
                /* Found optional country */
                tmpLocaleID = cntryID;
            }
            if(_isIDSeparator(*tmpLocaleID)) {
                /* If there was no country ID, skip a possible extra IDSeparator */
                if (tmpLocaleID != cntryID && _isIDSeparator(tmpLocaleID[1])) {
                    tmpLocaleID++;
                }
                i=_getVariant(tmpLocaleID+1, *tmpLocaleID, variant, variantCapacity);
            }
        }
    }

    /* removed by weiv. We don't want to handle POSIX variants anymore. Use canonicalization function */
    /* if we do not have a variant tag yet then try a POSIX variant after '@' */
/*
    if(!haveVariant && (localeID=uprv_strrchr(localeID, '@'))!=NULL) {
        i=_getVariant(localeID+1, '@', variant, variantCapacity);
    }
*/
    return u_terminateChars(variant, variantCapacity, i, err);
}

U_CAPI int32_t  U_EXPORT2
uloc_getName(const char* localeID,
             char* name,
             int32_t nameCapacity,
             UErrorCode* err)
{
    return _canonicalize(localeID, name, nameCapacity, 0, err);
}

U_CAPI int32_t  U_EXPORT2
uloc_getBaseName(const char* localeID,
                 char* name,
                 int32_t nameCapacity,
                 UErrorCode* err)
{
    return _canonicalize(localeID, name, nameCapacity, _ULOC_STRIP_KEYWORDS, err);
}

U_CAPI int32_t  U_EXPORT2
uloc_canonicalize(const char* localeID,
                  char* name,
                  int32_t nameCapacity,
                  UErrorCode* err)
{
    return _canonicalize(localeID, name, nameCapacity, _ULOC_CANONICALIZE, err);
}

U_CAPI const char*  U_EXPORT2
uloc_getISO3Language(const char* localeID)
{
    int16_t offset;
    char lang[ULOC_LANG_CAPACITY];
    UErrorCode err = U_ZERO_ERROR;

    if (localeID == NULL)
    {
        localeID = uloc_getDefault();
    }
    uloc_getLanguage(localeID, lang, ULOC_LANG_CAPACITY, &err);
    if (U_FAILURE(err))
        return "";
    offset = _findIndex(LANGUAGES, lang);
    if (offset < 0)
        return "";
    return LANGUAGES_3[offset];
}

U_CAPI const char*  U_EXPORT2
uloc_getISO3Country(const char* localeID)
{
    int16_t offset;
    char cntry[ULOC_LANG_CAPACITY];
    UErrorCode err = U_ZERO_ERROR;

    if (localeID == NULL)
    {
        localeID = uloc_getDefault();
    }
    uloc_getCountry(localeID, cntry, ULOC_LANG_CAPACITY, &err);
    if (U_FAILURE(err))
        return "";
    offset = _findIndex(COUNTRIES, cntry);
    if (offset < 0)
        return "";

    return COUNTRIES_3[offset];
}

U_CAPI uint32_t  U_EXPORT2
uloc_getLCID(const char* localeID)
{
    UErrorCode status = U_ZERO_ERROR;
    char       langID[ULOC_FULLNAME_CAPACITY];

    uloc_getLanguage(localeID, langID, sizeof(langID), &status);
    if (U_FAILURE(status)) {
        return 0;
    }

    if (uprv_strchr(localeID, '@')) {
        // uprv_convertToLCID does not support keywords other than collation.
        // Remove all keywords except collation.
        int32_t len;
        char collVal[ULOC_KEYWORDS_CAPACITY];
        char tmpLocaleID[ULOC_FULLNAME_CAPACITY];

        len = uloc_getKeywordValue(localeID, "collation", collVal,
            UPRV_LENGTHOF(collVal) - 1, &status);

        if (U_SUCCESS(status) && len > 0) {
            collVal[len] = 0;

            len = uloc_getBaseName(localeID, tmpLocaleID,
                UPRV_LENGTHOF(tmpLocaleID) - 1, &status);

            if (U_SUCCESS(status) && len > 0) {
                tmpLocaleID[len] = 0;

                len = uloc_setKeywordValue("collation", collVal, tmpLocaleID,
                    UPRV_LENGTHOF(tmpLocaleID) - len - 1, &status);

                if (U_SUCCESS(status) && len > 0) {
                    tmpLocaleID[len] = 0;
                    return uprv_convertToLCID(langID, tmpLocaleID, &status);
                }
            }
        }

        // fall through - all keywords are simply ignored
        status = U_ZERO_ERROR;
    }

    return uprv_convertToLCID(langID, localeID, &status);
}

U_CAPI int32_t U_EXPORT2
uloc_getLocaleForLCID(uint32_t hostid, char *locale, int32_t localeCapacity,
                UErrorCode *status)
{
    return uprv_convertToPosix(hostid, locale, localeCapacity, status);
}

/* ### Default locale **************************************************/

U_CAPI const char*  U_EXPORT2
uloc_getDefault()
{
    return locale_get_default();
}

U_CAPI void  U_EXPORT2
uloc_setDefault(const char*   newDefaultLocale,
             UErrorCode* err)
{
    if (U_FAILURE(*err))
        return;
    /* the error code isn't currently used for anything by this function*/

    /* propagate change to C++ */
    locale_set_default(newDefaultLocale);
}

/**
 * Returns a list of all 2-letter language codes defined in ISO 639.  This is a pointer
 * to an array of pointers to arrays of char.  All of these pointers are owned
 * by ICU-- do not delete them, and do not write through them.  The array is
 * terminated with a null pointer.
 */
U_CAPI const char* const*  U_EXPORT2
uloc_getISOLanguages()
{
    return LANGUAGES;
}

/**
 * Returns a list of all 2-letter country codes defined in ISO 639.  This is a
 * pointer to an array of pointers to arrays of char.  All of these pointers are
 * owned by ICU-- do not delete them, and do not write through them.  The array is
 * terminated with a null pointer.
 */
U_CAPI const char* const*  U_EXPORT2
uloc_getISOCountries()
{
    return COUNTRIES;
}


/* this function to be moved into cstring.c later */
static char gDecimal = 0;

static /* U_CAPI */
double
/* U_EXPORT2 */
_uloc_strtod(const char *start, char **end) {
    char *decimal;
    char *myEnd;
    char buf[30];
    double rv;
    if (!gDecimal) {
        char rep[5];
        /* For machines that decide to change the decimal on you,
        and try to be too smart with localization.
        This normally should be just a '.'. */
        sprintf(rep, "%+1.1f", 1.0);
        gDecimal = rep[2];
    }

    if(gDecimal == '.') {
        return uprv_strtod(start, end); /* fall through to OS */
    } else {
        uprv_strncpy(buf, start, 29);
        buf[29]=0;
        decimal = uprv_strchr(buf, '.');
        if(decimal) {
            *decimal = gDecimal;
        } else {
            return uprv_strtod(start, end); /* no decimal point */
        }
        rv = uprv_strtod(buf, &myEnd);
        if(end) {
            *end = (char*)(start+(myEnd-buf)); /* cast away const (to follow uprv_strtod API.) */
        }
        return rv;
    }
}

typedef struct {
    float q;
    int32_t dummy;  /* to avoid uninitialized memory copy from qsort */
    char locale[ULOC_FULLNAME_CAPACITY+1];
} _acceptLangItem;

static int32_t U_CALLCONV
uloc_acceptLanguageCompare(const void * /*context*/, const void *a, const void *b)
{
    const _acceptLangItem *aa = (const _acceptLangItem*)a;
    const _acceptLangItem *bb = (const _acceptLangItem*)b;

    int32_t rc = 0;
    if(bb->q < aa->q) {
        rc = -1;  /* A > B */
    } else if(bb->q > aa->q) {
        rc = 1;   /* A < B */
    } else {
        rc = 0;   /* A = B */
    }

    if(rc==0) {
        rc = uprv_stricmp(aa->locale, bb->locale);
    }

#if defined(ULOC_DEBUG)
    /*  fprintf(stderr, "a:[%s:%g], b:[%s:%g] -> %d\n",
    aa->locale, aa->q,
    bb->locale, bb->q,
    rc);*/
#endif

    return rc;
}

/*
mt-mt, ja;q=0.76, en-us;q=0.95, en;q=0.92, en-gb;q=0.89, fr;q=0.87, iu-ca;q=0.84, iu;q=0.82, ja-jp;q=0.79, mt;q=0.97, de-de;q=0.74, de;q=0.71, es;q=0.68, it-it;q=0.66, it;q=0.63, vi-vn;q=0.61, vi;q=0.58, nl-nl;q=0.55, nl;q=0.53
*/

U_CAPI int32_t U_EXPORT2
uloc_acceptLanguageFromHTTP(char *result, int32_t resultAvailable, UAcceptResult *outResult,
                            const char *httpAcceptLanguage,
                            UEnumeration* availableLocales,
                            UErrorCode *status)
{
  MaybeStackArray<_acceptLangItem, 4> items; // Struct for collecting items.
    char tmp[ULOC_FULLNAME_CAPACITY +1];
    int32_t n = 0;
    const char *itemEnd;
    const char *paramEnd;
    const char *s;
    const char *t;
    int32_t res;
    int32_t i;
    int32_t l = (int32_t)uprv_strlen(httpAcceptLanguage);

    if(U_FAILURE(*status)) {
        return -1;
    }

    for(s=httpAcceptLanguage;s&&*s;) {
        while(isspace(*s)) /* eat space at the beginning */
            s++;
        itemEnd=uprv_strchr(s,',');
        paramEnd=uprv_strchr(s,';');
        if(!itemEnd) {
            itemEnd = httpAcceptLanguage+l; /* end of string */
        }
        if(paramEnd && paramEnd<itemEnd) {
            /* semicolon (;) is closer than end (,) */
            t = paramEnd+1;
            if(*t=='q') {
                t++;
            }
            while(isspace(*t)) {
                t++;
            }
            if(*t=='=') {
                t++;
            }
            while(isspace(*t)) {
                t++;
            }
            items[n].q = (float)_uloc_strtod(t,NULL);
        } else {
            /* no semicolon - it's 1.0 */
            items[n].q = 1.0f;
            paramEnd = itemEnd;
        }
        items[n].dummy=0;
        /* eat spaces prior to semi */
        for(t=(paramEnd-1);(paramEnd>s)&&isspace(*t);t--)
            ;
        int32_t slen = ((t+1)-s);
        if(slen > ULOC_FULLNAME_CAPACITY) {
          *status = U_BUFFER_OVERFLOW_ERROR;
          return -1; // too big
        }
        uprv_strncpy(items[n].locale, s, slen);
        items[n].locale[slen]=0; // terminate
        int32_t clen = uloc_canonicalize(items[n].locale, tmp, UPRV_LENGTHOF(tmp)-1, status);
        if(U_FAILURE(*status)) return -1;
        if((clen!=slen) || (uprv_strncmp(items[n].locale, tmp, slen))) {
            // canonicalization had an effect- copy back
            uprv_strncpy(items[n].locale, tmp, clen);
            items[n].locale[clen] = 0; // terminate
        }
#if defined(ULOC_DEBUG)
        /*fprintf(stderr,"%d: s <%s> q <%g>\n", n, j[n].locale, j[n].q);*/
#endif
        n++;
        s = itemEnd;
        while(*s==',') { /* eat duplicate commas */
            s++;
        }
        if(n>=items.getCapacity()) { // If we need more items
          if(NULL == items.resize(items.getCapacity()*2, items.getCapacity())) {
              *status = U_MEMORY_ALLOCATION_ERROR;
              return -1;
          }
#if defined(ULOC_DEBUG)
          fprintf(stderr,"malloced at size %d\n", items.getCapacity());
#endif
        }
    }
    uprv_sortArray(items.getAlias(), n, sizeof(items[0]), uloc_acceptLanguageCompare, NULL, TRUE, status);
    if (U_FAILURE(*status)) {
        return -1;
    }
    LocalMemory<const char*> strs(NULL);
    if (strs.allocateInsteadAndReset(n) == NULL) {
        *status = U_MEMORY_ALLOCATION_ERROR;
        return -1;
    }
    for(i=0;i<n;i++) {
#if defined(ULOC_DEBUG)
        /*fprintf(stderr,"%d: s <%s> q <%g>\n", i, j[i].locale, j[i].q);*/
#endif
        strs[i]=items[i].locale;
    }
    res =  uloc_acceptLanguage(result, resultAvailable, outResult,
                               strs.getAlias(), n, availableLocales, status);
    return res;
}


U_CAPI int32_t U_EXPORT2
uloc_acceptLanguage(char *result, int32_t resultAvailable,
                    UAcceptResult *outResult, const char **acceptList,
                    int32_t acceptListCount,
                    UEnumeration* availableLocales,
                    UErrorCode *status)
{
    int32_t i,j;
    int32_t len;
    int32_t maxLen=0;
    char tmp[ULOC_FULLNAME_CAPACITY+1];
    const char *l;
    char **fallbackList;
    if(U_FAILURE(*status)) {
        return -1;
    }
    fallbackList = static_cast<char **>(uprv_malloc((size_t)(sizeof(fallbackList[0])*acceptListCount)));
    if(fallbackList==NULL) {
        *status = U_MEMORY_ALLOCATION_ERROR;
        return -1;
    }
    for(i=0;i<acceptListCount;i++) {
#if defined(ULOC_DEBUG)
        fprintf(stderr,"%02d: %s\n", i, acceptList[i]);
#endif
        while((l=uenum_next(availableLocales, NULL, status))) {
#if defined(ULOC_DEBUG)
            fprintf(stderr,"  %s\n", l);
#endif
            len = (int32_t)uprv_strlen(l);
            if(!uprv_strcmp(acceptList[i], l)) {
                if(outResult) {
                    *outResult = ULOC_ACCEPT_VALID;
                }
#if defined(ULOC_DEBUG)
                fprintf(stderr, "MATCH! %s\n", l);
#endif
                if(len>0) {
                    uprv_strncpy(result, l, uprv_min(len, resultAvailable));
                }
                for(j=0;j<i;j++) {
                    uprv_free(fallbackList[j]);
                }
                uprv_free(fallbackList);
                return u_terminateChars(result, resultAvailable, len, status);
            }
            if(len>maxLen) {
                maxLen = len;
            }
        }
        uenum_reset(availableLocales, status);
        /* save off parent info */
        if(uloc_getParent(acceptList[i], tmp, UPRV_LENGTHOF(tmp), status)!=0) {
            fallbackList[i] = uprv_strdup(tmp);
        } else {
            fallbackList[i]=0;
        }
    }

    for(maxLen--;maxLen>0;maxLen--) {
        for(i=0;i<acceptListCount;i++) {
            if(fallbackList[i] && ((int32_t)uprv_strlen(fallbackList[i])==maxLen)) {
#if defined(ULOC_DEBUG)
                fprintf(stderr,"Try: [%s]", fallbackList[i]);
#endif
                while((l=uenum_next(availableLocales, NULL, status))) {
#if defined(ULOC_DEBUG)
                    fprintf(stderr,"  %s\n", l);
#endif
                    len = (int32_t)uprv_strlen(l);
                    if(!uprv_strcmp(fallbackList[i], l)) {
                        if(outResult) {
                            *outResult = ULOC_ACCEPT_FALLBACK;
                        }
#if defined(ULOC_DEBUG)
                        fprintf(stderr, "fallback MATCH! %s\n", l);
#endif
                        if(len>0) {
                            uprv_strncpy(result, l, uprv_min(len, resultAvailable));
                        }
                        for(j=0;j<acceptListCount;j++) {
                            uprv_free(fallbackList[j]);
                        }
                        uprv_free(fallbackList);
                        return u_terminateChars(result, resultAvailable, len, status);
                    }
                }
                uenum_reset(availableLocales, status);

                if(uloc_getParent(fallbackList[i], tmp, UPRV_LENGTHOF(tmp), status)!=0) {
                    uprv_free(fallbackList[i]);
                    fallbackList[i] = uprv_strdup(tmp);
                } else {
                    uprv_free(fallbackList[i]);
                    fallbackList[i]=0;
                }
            }
        }
        if(outResult) {
            *outResult = ULOC_ACCEPT_FAILED;
        }
    }
    for(i=0;i<acceptListCount;i++) {
        uprv_free(fallbackList[i]);
    }
    uprv_free(fallbackList);
    return -1;
}

U_CAPI const char* U_EXPORT2
uloc_toUnicodeLocaleKey(const char* keyword)
{
    const char* bcpKey = ulocimp_toBcpKey(keyword);
    if (bcpKey == NULL && ultag_isUnicodeLocaleKey(keyword, -1)) {
        // unknown keyword, but syntax is fine..
        return keyword;
    }
    return bcpKey;
}

U_CAPI const char* U_EXPORT2
uloc_toUnicodeLocaleType(const char* keyword, const char* value)
{
    const char* bcpType = ulocimp_toBcpType(keyword, value, NULL, NULL);
    if (bcpType == NULL && ultag_isUnicodeLocaleType(value, -1)) {
        // unknown keyword, but syntax is fine..
        return value;
    }
    return bcpType;
}

#define UPRV_ISDIGIT(c) (((c) >= '0') && ((c) <= '9'))
#define UPRV_ISALPHANUM(c) (uprv_isASCIILetter(c) || UPRV_ISDIGIT(c) )

static UBool
isWellFormedLegacyKey(const char* legacyKey)
{
    const char* p = legacyKey;
    while (*p) {
        if (!UPRV_ISALPHANUM(*p)) {
            return FALSE;
        }
        p++;
    }
    return TRUE;
}

static UBool
isWellFormedLegacyType(const char* legacyType)
{
    const char* p = legacyType;
    int32_t alphaNumLen = 0;
    while (*p) {
        if (*p == '_' || *p == '/' || *p == '-') {
            if (alphaNumLen == 0) {
                return FALSE;
            }
            alphaNumLen = 0;
        } else if (UPRV_ISALPHANUM(*p)) {
            alphaNumLen++;
        } else {
            return FALSE;
        }
        p++;
    }
    return (alphaNumLen != 0);
}

U_CAPI const char* U_EXPORT2
uloc_toLegacyKey(const char* keyword)
{
    const char* legacyKey = ulocimp_toLegacyKey(keyword);
    if (legacyKey == NULL) {
        // Checks if the specified locale key is well-formed with the legacy locale syntax.
        //
        // Note:
        //  Neither ICU nor LDML/CLDR provides the definition of keyword syntax.
        //  However, a key should not contain '=' obviously. For now, all existing
        //  keys are using ASCII alphabetic letters only. We won't add any new key
        //  that is not compatible with the BCP 47 syntax. Therefore, we assume
        //  a valid key consist from [0-9a-zA-Z], no symbols.
        if (isWellFormedLegacyKey(keyword)) {
            return keyword;
        }
    }
    return legacyKey;
}

U_CAPI const char* U_EXPORT2
uloc_toLegacyType(const char* keyword, const char* value)
{
    const char* legacyType = ulocimp_toLegacyType(keyword, value, NULL, NULL);
    if (legacyType == NULL) {
        // Checks if the specified locale type is well-formed with the legacy locale syntax.
        //
        // Note:
        //  Neither ICU nor LDML/CLDR provides the definition of keyword syntax.
        //  However, a type should not contain '=' obviously. For now, all existing
        //  types are using ASCII alphabetic letters with a few symbol letters. We won't
        //  add any new type that is not compatible with the BCP 47 syntax except timezone
        //  IDs. For now, we assume a valid type start with [0-9a-zA-Z], but may contain
        //  '-' '_' '/' in the middle.
        if (isWellFormedLegacyType(value)) {
            return value;
        }
    }
    return legacyType;
}

/*eof*/
