// © 2016 and later: Unicode, Inc. and others.
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

#include "unicode/bytestream.h"
#include "unicode/errorcode.h"
#include "unicode/stringpiece.h"
#include "unicode/utypes.h"
#include "unicode/ustring.h"
#include "unicode/uloc.h"

#include "bytesinkutil.h"
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
#include "charstr.h"

U_NAMESPACE_USE

/* ### Declarations **************************************************/

/* Locale stuff from locid.cpp */
U_CFUNC void locale_set_default(const char *id);
U_CFUNC const char *locale_get_default();

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
 * This table should be terminated with a nullptr entry, followed by a
 * second list, and another nullptr entry.  The first list is visible to
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
/* Subsequent hand addition of selected languages */
static const char * const LANGUAGES[] = {
    "aa",  "ab",  "ace", "ach", "ada", "ady", "ae",  "aeb",
    "af",  "afh", "agq", "ain", "ak",  "akk", "akz", "ale",
    "aln", "alt", "am",  "an",  "ang", "anp", "ar",  "arc",
    "arn", "aro", "arp", "arq", "ars", "arw", "ary", "arz", "as",
    "asa", "ase", "ast", "av",  "avk", "awa", "ay",  "az",
    "ba",  "bal", "ban", "bar", "bas", "bax", "bbc", "bbj",
    "be",  "bej", "bem", "bew", "bez", "bfd", "bfq", "bg",
    "bgc", "bgn", "bho", "bi",  "bik", "bin", "bjn", "bkm", "bla",
    "blo", "bm",  "bn",  "bo",  "bpy", "bqi", "br",  "bra", "brh",
    "brx", "bs",  "bss", "bua", "bug", "bum", "byn", "byv",
    "ca",  "cad", "car", "cay", "cch", "ccp", "ce",  "ceb", "cgg",
    "ch",  "chb", "chg", "chk", "chm", "chn", "cho", "chp",
    "chr", "chy", "ckb", "co",  "cop", "cps", "cr",  "crh",
    "cs",  "csb", "csw", "cu",  "cv",  "cy",
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
    "kv",  "kw",  "kxv", "ky",
    "la",  "lad", "lag", "lah", "lam", "lb",  "lez", "lfn",
    "lg",  "li",  "lij", "liv", "lkt", "lmo", "ln",  "lo",
    "lol", "loz", "lrc", "lt",  "ltg", "lu",  "lua", "lui",
    "lun", "luo", "lus", "luy", "lv",  "lzh", "lzz",
    "mad", "maf", "mag", "mai", "mak", "man", "mas", "mde",
    "mdf", "mdh", "mdr", "men", "mer", "mfe", "mg",  "mga",
    "mgh", "mgo", "mh",  "mi",  "mic", "min", "mis", "mk",
    "ml",  "mn",  "mnc", "mni",
    "moh", "mos", "mr",  "mrj",
    "ms",  "mt",  "mua", "mul", "mus", "mwl", "mwr", "mwv",
    "my",  "mye", "myv", "mzn",
    "na",  "nan", "nap", "naq", "nb",  "nd",  "nds", "ne",
    "new", "ng",  "nia", "niu", "njo", "nl",  "nmg", "nn",
    "nnh", "no",  "nog", "non", "nov", "nqo", "nr",  "nso",
    "nus", "nv",  "nwc", "ny",  "nym", "nyn", "nyo", "nzi",
    "oc",  "oj",  "om",  "or",  "os",  "osa", "ota",
    "pa",  "pag", "pal", "pam", "pap", "pau", "pcd", "pcm", "pdc",
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
    "sv",  "sw",  "swb", "syc", "syr", "szl",
    "ta",  "tcy", "te",  "tem", "teo", "ter", "tet", "tg",
    "th",  "ti",  "tig", "tiv", "tk",  "tkl", "tkr",
    "tlh", "tli", "tly", "tmh", "tn",  "to",  "tog", "tok", "tpi",
    "tr",  "tru", "trv", "ts",  "tsd", "tsi", "tt",  "ttt",
    "tum", "tvl", "tw",  "twq", "ty",  "tyv", "tzm",
    "udm", "ug",  "uga", "uk",  "umb", "und", "ur",  "uz",
    "vai", "ve",  "vec", "vep", "vi",  "vls", "vmf", "vmw",
    "vo", "vot", "vro", "vun",
    "wa",  "wae", "wal", "war", "was", "wbp", "wo",  "wuu",
    "xal", "xh",  "xmf", "xnr", "xog",
    "yao", "yap", "yav", "ybb", "yi",  "yo",  "yrl", "yue",
    "za",  "zap", "zbl", "zea", "zen", "zgh", "zh",  "zu",
    "zun", "zxx", "zza",
nullptr,
    "in",  "iw",  "ji",  "jw",  "mo",  "sh",  "swc", "tl",  /* obsolete language codes */
nullptr
};

static const char* const DEPRECATED_LANGUAGES[]={
    "in", "iw", "ji", "jw", "mo", nullptr, nullptr
};
static const char* const REPLACEMENT_LANGUAGES[]={
    "id", "he", "yi", "jv", "ro", nullptr, nullptr
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
 * This table should be terminated with a nullptr entry, followed by a
 * second list, and another nullptr entry.  The two lists correspond to
 * the two lists in LANGUAGES.
 */
/* Generated using org.unicode.cldr.icu.GenerateISO639LanguageTables */
/* ISO639 table version is 20150505 */
/* Subsequent hand addition of selected languages */
static const char * const LANGUAGES_3[] = {
    "aar", "abk", "ace", "ach", "ada", "ady", "ave", "aeb",
    "afr", "afh", "agq", "ain", "aka", "akk", "akz", "ale",
    "aln", "alt", "amh", "arg", "ang", "anp", "ara", "arc",
    "arn", "aro", "arp", "arq", "ars", "arw", "ary", "arz", "asm",
    "asa", "ase", "ast", "ava", "avk", "awa", "aym", "aze",
    "bak", "bal", "ban", "bar", "bas", "bax", "bbc", "bbj",
    "bel", "bej", "bem", "bew", "bez", "bfd", "bfq", "bul",
    "bgc", "bgn", "bho", "bis", "bik", "bin", "bjn", "bkm", "bla",
    "blo", "bam", "ben", "bod", "bpy", "bqi", "bre", "bra", "brh",
    "brx", "bos", "bss", "bua", "bug", "bum", "byn", "byv",
    "cat", "cad", "car", "cay", "cch", "ccp", "che", "ceb", "cgg",
    "cha", "chb", "chg", "chk", "chm", "chn", "cho", "chp",
    "chr", "chy", "ckb", "cos", "cop", "cps", "cre", "crh",
    "ces", "csb", "csw", "chu", "chv", "cym",
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
    "kom", "cor", "kxv", "kir",
    "lat", "lad", "lag", "lah", "lam", "ltz", "lez", "lfn",
    "lug", "lim", "lij", "liv", "lkt", "lmo", "lin", "lao",
    "lol", "loz", "lrc", "lit", "ltg", "lub", "lua", "lui",
    "lun", "luo", "lus", "luy", "lav", "lzh", "lzz",
    "mad", "maf", "mag", "mai", "mak", "man", "mas", "mde",
    "mdf", "mdh", "mdr", "men", "mer", "mfe", "mlg", "mga",
    "mgh", "mgo", "mah", "mri", "mic", "min", "mis", "mkd",
    "mal", "mon", "mnc", "mni",
    "moh", "mos", "mar", "mrj",
    "msa", "mlt", "mua", "mul", "mus", "mwl", "mwr", "mwv",
    "mya", "mye", "myv", "mzn",
    "nau", "nan", "nap", "naq", "nob", "nde", "nds", "nep",
    "new", "ndo", "nia", "niu", "njo", "nld", "nmg", "nno",
    "nnh", "nor", "nog", "non", "nov", "nqo", "nbl", "nso",
    "nus", "nav", "nwc", "nya", "nym", "nyn", "nyo", "nzi",
    "oci", "oji", "orm", "ori", "oss", "osa", "ota",
    "pan", "pag", "pal", "pam", "pap", "pau", "pcd", "pcm", "pdc",
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
    "swe", "swa", "swb", "syc", "syr", "szl",
    "tam", "tcy", "tel", "tem", "teo", "ter", "tet", "tgk",
    "tha", "tir", "tig", "tiv", "tuk", "tkl", "tkr",
    "tlh", "tli", "tly", "tmh", "tsn", "ton", "tog", "tok", "tpi",
    "tur", "tru", "trv", "tso", "tsd", "tsi", "tat", "ttt",
    "tum", "tvl", "twi", "twq", "tah", "tyv", "tzm",
    "udm", "uig", "uga", "ukr", "umb", "und", "urd", "uzb",
    "vai", "ven", "vec", "vep", "vie", "vls", "vmf", "vmw",
    "vol", "vot", "vro", "vun",
    "wln", "wae", "wal", "war", "was", "wbp", "wol", "wuu",
    "xal", "xho", "xmf", "xnr", "xog",
    "yao", "yap", "yav", "ybb", "yid", "yor", "yrl", "yue",
    "zha", "zap", "zbl", "zea", "zen", "zgh", "zho", "zul",
    "zun", "zxx", "zza",
nullptr,
/*  "in",  "iw",  "ji",  "jw",  "mo",  "sh",  "swc", "tl",  */
    "ind", "heb", "yid", "jaw", "mol", "srp", "swc", "tgl",
nullptr
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
 * This table should be terminated with a nullptr entry, followed by a
 * second list, and another nullptr entry.  The first list is visible to
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
    "CH",  "CI",  "CK",  "CL",  "CM",  "CN",  "CO",  "CQ",  "CR",
    "CU",  "CV",  "CW",  "CX",  "CY",  "CZ",  "DE",  "DG",  "DJ",  "DK",
    "DM",  "DO",  "DZ",  "EA",  "EC",  "EE",  "EG",  "EH",  "ER",
    "ES",  "ET",  "FI",  "FJ",  "FK",  "FM",  "FO",  "FR",
    "GA",  "GB",  "GD",  "GE",  "GF",  "GG",  "GH",  "GI",  "GL",
    "GM",  "GN",  "GP",  "GQ",  "GR",  "GS",  "GT",  "GU",
    "GW",  "GY",  "HK",  "HM",  "HN",  "HR",  "HT",  "HU",
    "IC",  "ID",  "IE",  "IL",  "IM",  "IN",  "IO",  "IQ",  "IR",  "IS",
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
    "WS",  "XK",  "YE",  "YT",  "ZA",  "ZM",  "ZW",
nullptr,
    "AN",  "BU", "CS", "FX", "RO", "SU", "TP", "YD", "YU", "ZR",   /* obsolete country codes */
nullptr
};

static const char* const DEPRECATED_COUNTRIES[] = {
    "AN", "BU", "CS", "DD", "DY", "FX", "HV", "NH", "RH", "SU", "TP", "UK", "VD", "YD", "YU", "ZR", nullptr, nullptr /* deprecated country list */
};
static const char* const REPLACEMENT_COUNTRIES[] = {
/*  "AN", "BU", "CS", "DD", "DY", "FX", "HV", "NH", "RH", "SU", "TP", "UK", "VD", "YD", "YU", "ZR" */
    "CW", "MM", "RS", "DE", "BJ", "FR", "BF", "VU", "ZW", "RU", "TL", "GB", "VN", "YE", "RS", "CD", nullptr, nullptr  /* replacement country codes */
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
 * This table should be terminated with a nullptr entry, followed by a
 * second list, and another nullptr entry.  The two lists correspond to
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
/*  "CH",  "CI",  "CK",  "CL",  "CM",  "CN",  "CO",  "CQ",  "CR",     */
    "CHE", "CIV", "COK", "CHL", "CMR", "CHN", "COL", "CRQ", "CRI",
/*  "CU",  "CV",  "CW",  "CX",  "CY",  "CZ",  "DE",  "DG",  "DJ",  "DK",     */
    "CUB", "CPV", "CUW", "CXR", "CYP", "CZE", "DEU", "DGA", "DJI", "DNK",
/*  "DM",  "DO",  "DZ",  "EA",  "EC",  "EE",  "EG",  "EH",  "ER",     */
    "DMA", "DOM", "DZA", "XEA", "ECU", "EST", "EGY", "ESH", "ERI",
/*  "ES",  "ET",  "FI",  "FJ",  "FK",  "FM",  "FO",  "FR",     */
    "ESP", "ETH", "FIN", "FJI", "FLK", "FSM", "FRO", "FRA",
/*  "GA",  "GB",  "GD",  "GE",  "GF",  "GG",  "GH",  "GI",  "GL",     */
    "GAB", "GBR", "GRD", "GEO", "GUF", "GGY", "GHA", "GIB", "GRL",
/*  "GM",  "GN",  "GP",  "GQ",  "GR",  "GS",  "GT",  "GU",     */
    "GMB", "GIN", "GLP", "GNQ", "GRC", "SGS", "GTM", "GUM",
/*  "GW",  "GY",  "HK",  "HM",  "HN",  "HR",  "HT",  "HU",     */
    "GNB", "GUY", "HKG", "HMD", "HND", "HRV", "HTI", "HUN",
/*  "IC",  "ID",  "IE",  "IL",  "IM",  "IN",  "IO",  "IQ",  "IR",  "IS" */
    "XIC", "IDN", "IRL", "ISR", "IMN", "IND", "IOT", "IRQ", "IRN", "ISL",
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
/*  "WS",  "XK",  "YE",  "YT",  "ZA",  "ZM",  "ZW",          */
    "WSM", "XKK", "YEM", "MYT", "ZAF", "ZMB", "ZWE",
nullptr,
/*  "AN",  "BU",  "CS",  "FX",  "RO", "SU",  "TP",  "YD",  "YU",  "ZR" */
    "ANT", "BUR", "SCG", "FXX", "ROM", "SUN", "TMP", "YMD", "YUG", "ZAR",
nullptr
};

typedef struct CanonicalizationMap {
    const char *id;          /* input ID */
    const char *canonicalID; /* canonicalized output ID */
} CanonicalizationMap;

/**
 * A map to canonicalize locale IDs.  This handles a variety of
 * different semantic kinds of transformations.
 */
static const CanonicalizationMap CANONICALIZE_MAP[] = {
    { "art__LOJBAN",    "jbo" }, /* registered name */
    { "hy__AREVELA",    "hy" }, /* Registered IANA variant */
    { "hy__AREVMDA",    "hyw" }, /* Registered IANA variant */
    { "zh__GUOYU",      "zh" }, /* registered name */
    { "zh__HAKKA",      "hak" }, /* registered name */
    { "zh__XIANG",      "hsn" }, /* registered name */
    // subtags with 3 chars won't be treated as variants.
    { "zh_GAN",         "gan" }, /* registered name */
    { "zh_MIN_NAN",     "nan" }, /* registered name */
    { "zh_WUU",         "wuu" }, /* registered name */
    { "zh_YUE",         "yue" }, /* registered name */
};

/* ### BCP47 Conversion *******************************************/
/* Test if the locale id has BCP47 u extension and does not have '@' */
#define _hasBCP47Extension(id) (id && uprv_strstr(id, "@") == nullptr && getShortestSubtagLength(localeID) == 1)
/* Gets the size of the shortest subtag in the given localeID. */
static int32_t getShortestSubtagLength(const char *localeID) {
    int32_t localeIDLength = static_cast<int32_t>(uprv_strlen(localeID));
    int32_t length = localeIDLength;
    int32_t tmpLength = 0;
    int32_t i;
    UBool reset = true;

    for (i = 0; i < localeIDLength; i++) {
        if (localeID[i] != '_' && localeID[i] != '-') {
            if (reset) {
                tmpLength = 0;
                reset = false;
            }
            tmpLength++;
        } else {
            if (tmpLength != 0 && tmpLength < length) {
                length = tmpLength;
            }
            reset = true;
        }
    }

    return length;
}

/* ### Keywords **************************************************/
#define UPRV_ISDIGIT(c) (((c) >= '0') && ((c) <= '9'))
#define UPRV_ISALPHANUM(c) (uprv_isASCIILetter(c) || UPRV_ISDIGIT(c) )
/* Punctuation/symbols allowed in legacy key values */
#define UPRV_OK_VALUE_PUNCTUATION(c) ((c) == '_' || (c) == '-' || (c) == '+' || (c) == '/')

#define ULOC_KEYWORD_BUFFER_LEN 25
#define ULOC_MAX_NO_KEYWORDS 25

U_CAPI const char * U_EXPORT2
locale_getKeywordsStart(const char *localeID) {
    const char *result = nullptr;
    if((result = uprv_strchr(localeID, '@')) != nullptr) {
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
            if((result = uprv_strchr(localeID, *charToFind)) != nullptr) {
                return result;
            }
            charToFind++;
        }
    }
#endif
    return nullptr;
}

/**
 * @param buf buffer of size [ULOC_KEYWORD_BUFFER_LEN]
 * @param keywordName incoming name to be canonicalized
 * @param status return status (keyword too long)
 * @return length of the keyword name
 */
static int32_t locale_canonKeywordName(char *buf, const char *keywordName, UErrorCode *status)
{
  int32_t keywordNameLen = 0;

  for (; *keywordName != 0; keywordName++) {
    if (!UPRV_ISALPHANUM(*keywordName)) {
      *status = U_ILLEGAL_ARGUMENT_ERROR; /* malformed keyword name */
      return 0;
    }
    if (keywordNameLen < ULOC_KEYWORD_BUFFER_LEN - 1) {
      buf[keywordNameLen++] = uprv_tolower(*keywordName);
    } else {
      /* keyword name too long for internal buffer */
      *status = U_INTERNAL_PROGRAM_ERROR;
      return 0;
    }
  }
  if (keywordNameLen == 0) {
    *status = U_ILLEGAL_ARGUMENT_ERROR; /* empty keyword name */
    return 0;
  }
  buf[keywordNameLen] = 0; /* terminate */

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

U_CFUNC void
ulocimp_getKeywords(const char *localeID,
                    char prev,
                    ByteSink& sink,
                    UBool valuesToo,
                    UErrorCode *status)
{
    KeywordStruct keywordList[ULOC_MAX_NO_KEYWORDS];

    int32_t maxKeywords = ULOC_MAX_NO_KEYWORDS;
    int32_t numKeywords = 0;
    const char* pos = localeID;
    const char* equalSign = nullptr;
    const char* semicolon = nullptr;
    int32_t i = 0, j, n;

    if(prev == '@') { /* start of keyword definition */
        /* we will grab pairs, trim spaces, lowercase keywords, sort and return */
        do {
            UBool duplicate = false;
            /* skip leading spaces */
            while(*pos == ' ') {
                pos++;
            }
            if (!*pos) { /* handle trailing "; " */
                break;
            }
            if(numKeywords == maxKeywords) {
                *status = U_INTERNAL_PROGRAM_ERROR;
                return;
            }
            equalSign = uprv_strchr(pos, '=');
            semicolon = uprv_strchr(pos, ';');
            /* lack of '=' [foo@currency] is illegal */
            /* ';' before '=' [foo@currency;collation=pinyin] is illegal */
            if(!equalSign || (semicolon && semicolon<equalSign)) {
                *status = U_INVALID_FORMAT_ERROR;
                return;
            }
            /* need to normalize both keyword and keyword name */
            if(equalSign - pos >= ULOC_KEYWORD_BUFFER_LEN) {
                /* keyword name too long for internal buffer */
                *status = U_INTERNAL_PROGRAM_ERROR;
                return;
            }
            for(i = 0, n = 0; i < equalSign - pos; ++i) {
                if (pos[i] != ' ') {
                    keywordList[numKeywords].keyword[n++] = uprv_tolower(pos[i]);
                }
            }

            /* zero-length keyword is an error. */
            if (n == 0) {
                *status = U_INVALID_FORMAT_ERROR;
                return;
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
                return;
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
                    duplicate = true;
                    break;
                }
            }
            if (!duplicate) {
                ++numKeywords;
            }
        } while(pos);

        /* now we have a list of keywords */
        /* we need to sort it */
        uprv_sortArray(keywordList, numKeywords, sizeof(KeywordStruct), compareKeywordStructs, nullptr, false, status);

        /* Now construct the keyword part */
        for(i = 0; i < numKeywords; i++) {
            sink.Append(keywordList[i].keyword, keywordList[i].keywordLen);
            if(valuesToo) {
                sink.Append("=", 1);
                sink.Append(keywordList[i].valueStart, keywordList[i].valueLen);
                if(i < numKeywords - 1) {
                    sink.Append(";", 1);
                }
            } else {
                sink.Append("\0", 1);
            }
        }
    }
}

U_CAPI int32_t U_EXPORT2
uloc_getKeywordValue(const char* localeID,
                     const char* keywordName,
                     char* buffer, int32_t bufferCapacity,
                     UErrorCode* status)
{
    if (U_FAILURE(*status)) {
        return 0;
    }

    CheckedArrayByteSink sink(buffer, bufferCapacity);
    ulocimp_getKeywordValue(localeID, keywordName, sink, status);

    int32_t reslen = sink.NumberOfBytesAppended();

    if (U_FAILURE(*status)) {
        return reslen;
    }

    if (sink.Overflowed()) {
        *status = U_BUFFER_OVERFLOW_ERROR;
    } else {
        u_terminateChars(buffer, bufferCapacity, reslen, status);
    }

    return reslen;
}

U_CAPI void U_EXPORT2
ulocimp_getKeywordValue(const char* localeID,
                        const char* keywordName,
                        icu::ByteSink& sink,
                        UErrorCode* status)
{
    const char* startSearchHere = nullptr;
    const char* nextSeparator = nullptr;
    char keywordNameBuffer[ULOC_KEYWORD_BUFFER_LEN];
    char localeKeywordNameBuffer[ULOC_KEYWORD_BUFFER_LEN];

    if(status && U_SUCCESS(*status) && localeID) {
      CharString tempBuffer;
      const char* tmpLocaleID;

      if (keywordName == nullptr || keywordName[0] == 0) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return;
      }

      locale_canonKeywordName(keywordNameBuffer, keywordName, status);
      if(U_FAILURE(*status)) {
        return;
      }

      if (_hasBCP47Extension(localeID)) {
        CharStringByteSink sink(&tempBuffer);
        ulocimp_forLanguageTag(localeID, -1, sink, nullptr, status);
        tmpLocaleID = U_SUCCESS(*status) && !tempBuffer.isEmpty() ? tempBuffer.data() : localeID;
      } else {
          tmpLocaleID=localeID;
      }

      startSearchHere = locale_getKeywordsStart(tmpLocaleID);
      if(startSearchHere == nullptr) {
          /* no keywords, return at once */
          return;
      }

      /* find the first keyword */
      while(startSearchHere) {
          const char* keyValueTail;
          int32_t keyValueLen;

          startSearchHere++; /* skip @ or ; */
          nextSeparator = uprv_strchr(startSearchHere, '=');
          if(!nextSeparator) {
              *status = U_ILLEGAL_ARGUMENT_ERROR; /* key must have =value */
              return;
          }
          /* strip leading & trailing spaces (TC decided to tolerate these) */
          while(*startSearchHere == ' ') {
              startSearchHere++;
          }
          keyValueTail = nextSeparator;
          while (keyValueTail > startSearchHere && *(keyValueTail-1) == ' ') {
              keyValueTail--;
          }
          /* now keyValueTail points to first char after the keyName */
          /* copy & normalize keyName from locale */
          if (startSearchHere == keyValueTail) {
              *status = U_ILLEGAL_ARGUMENT_ERROR; /* empty keyword name in passed-in locale */
              return;
          }
          keyValueLen = 0;
          while (startSearchHere < keyValueTail) {
            if (!UPRV_ISALPHANUM(*startSearchHere)) {
              *status = U_ILLEGAL_ARGUMENT_ERROR; /* malformed keyword name */
              return;
            }
            if (keyValueLen < ULOC_KEYWORD_BUFFER_LEN - 1) {
              localeKeywordNameBuffer[keyValueLen++] = uprv_tolower(*startSearchHere++);
            } else {
              /* keyword name too long for internal buffer */
              *status = U_INTERNAL_PROGRAM_ERROR;
              return;
            }
          }
          localeKeywordNameBuffer[keyValueLen] = 0; /* terminate */

          startSearchHere = uprv_strchr(nextSeparator, ';');

          if(uprv_strcmp(keywordNameBuffer, localeKeywordNameBuffer) == 0) {
               /* current entry matches the keyword. */
             nextSeparator++; /* skip '=' */
              /* First strip leading & trailing spaces (TC decided to tolerate these) */
              while(*nextSeparator == ' ') {
                nextSeparator++;
              }
              keyValueTail = (startSearchHere)? startSearchHere: nextSeparator + uprv_strlen(nextSeparator);
              while(keyValueTail > nextSeparator && *(keyValueTail-1) == ' ') {
                keyValueTail--;
              }
              /* Now copy the value, but check well-formedness */
              if (nextSeparator == keyValueTail) {
                *status = U_ILLEGAL_ARGUMENT_ERROR; /* empty key value name in passed-in locale */
                return;
              }
              while (nextSeparator < keyValueTail) {
                if (!UPRV_ISALPHANUM(*nextSeparator) && !UPRV_OK_VALUE_PUNCTUATION(*nextSeparator)) {
                  *status = U_ILLEGAL_ARGUMENT_ERROR; /* malformed key value */
                  return;
                }
                /* Should we lowercase value to return here? Tests expect as-is. */
                sink.Append(nextSeparator++, 1);
              }
              return;
          }
      }
    }
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
    char keywordNameBuffer[ULOC_KEYWORD_BUFFER_LEN];
    char keywordValueBuffer[ULOC_KEYWORDS_CAPACITY+1];
    char localeKeywordNameBuffer[ULOC_KEYWORD_BUFFER_LEN];
    int32_t rc;
    char* nextSeparator = nullptr;
    char* nextEqualsign = nullptr;
    char* startSearchHere = nullptr;
    char* keywordStart = nullptr;
    CharString updatedKeysAndValues;
    UBool handledInputKeyAndValue = false;
    char keyValuePrefix = '@';

    if(U_FAILURE(*status)) {
        return -1;
    }
    if (*status == U_STRING_NOT_TERMINATED_WARNING) {
        *status = U_ZERO_ERROR;
    }
    if (keywordName == nullptr || keywordName[0] == 0 || bufferCapacity <= 1) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }
    bufLen = (int32_t)uprv_strlen(buffer);
    if(bufferCapacity<bufLen) {
        /* The capacity is less than the length?! Is this NUL terminated? */
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }
    keywordNameLen = locale_canonKeywordName(keywordNameBuffer, keywordName, status);
    if(U_FAILURE(*status)) {
        return 0;
    }

    keywordValueLen = 0;
    if(keywordValue) {
        while (*keywordValue != 0) {
            if (!UPRV_ISALPHANUM(*keywordValue) && !UPRV_OK_VALUE_PUNCTUATION(*keywordValue)) {
                *status = U_ILLEGAL_ARGUMENT_ERROR; /* malformed key value */
                return 0;
            }
            if (keywordValueLen < ULOC_KEYWORDS_CAPACITY) {
                /* Should we force lowercase in value to set? */
                keywordValueBuffer[keywordValueLen++] = *keywordValue++;
            } else {
                /* keywordValue too long for internal buffer */
                *status = U_INTERNAL_PROGRAM_ERROR;
                return 0;
            }
        }
    }
    keywordValueBuffer[keywordValueLen] = 0; /* terminate */

    startSearchHere = (char*)locale_getKeywordsStart(buffer);
    if(startSearchHere == nullptr || (startSearchHere[1]==0)) {
        if(keywordValueLen == 0) { /* no keywords = nothing to remove */
            U_ASSERT(*status != U_STRING_NOT_TERMINATED_WARNING);
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
        *startSearchHere++ = '@';
        uprv_strcpy(startSearchHere, keywordNameBuffer);
        startSearchHere += keywordNameLen;
        *startSearchHere++ = '=';
        uprv_strcpy(startSearchHere, keywordValueBuffer);
        U_ASSERT(*status != U_STRING_NOT_TERMINATED_WARNING);
        return needLen;
    } /* end shortcut - no @ */

    keywordStart = startSearchHere;
    /* search for keyword */
    while(keywordStart) {
        const char* keyValueTail;
        int32_t keyValueLen;

        keywordStart++; /* skip @ or ; */
        nextEqualsign = uprv_strchr(keywordStart, '=');
        if (!nextEqualsign) {
            *status = U_ILLEGAL_ARGUMENT_ERROR; /* key must have =value */
            return 0;
        }
        /* strip leading & trailing spaces (TC decided to tolerate these) */
        while(*keywordStart == ' ') {
            keywordStart++;
        }
        keyValueTail = nextEqualsign;
        while (keyValueTail > keywordStart && *(keyValueTail-1) == ' ') {
            keyValueTail--;
        }
        /* now keyValueTail points to first char after the keyName */
        /* copy & normalize keyName from locale */
        if (keywordStart == keyValueTail) {
            *status = U_ILLEGAL_ARGUMENT_ERROR; /* empty keyword name in passed-in locale */
            return 0;
        }
        keyValueLen = 0;
        while (keywordStart < keyValueTail) {
            if (!UPRV_ISALPHANUM(*keywordStart)) {
                *status = U_ILLEGAL_ARGUMENT_ERROR; /* malformed keyword name */
                return 0;
            }
            if (keyValueLen < ULOC_KEYWORD_BUFFER_LEN - 1) {
                localeKeywordNameBuffer[keyValueLen++] = uprv_tolower(*keywordStart++);
            } else {
                /* keyword name too long for internal buffer */
                *status = U_INTERNAL_PROGRAM_ERROR;
                return 0;
            }
        }
        localeKeywordNameBuffer[keyValueLen] = 0; /* terminate */

        nextSeparator = uprv_strchr(nextEqualsign, ';');

        /* start processing the value part */
        nextEqualsign++; /* skip '=' */
        /* First strip leading & trailing spaces (TC decided to tolerate these) */
        while(*nextEqualsign == ' ') {
            nextEqualsign++;
        }
        keyValueTail = (nextSeparator)? nextSeparator: nextEqualsign + uprv_strlen(nextEqualsign);
        while(keyValueTail > nextEqualsign && *(keyValueTail-1) == ' ') {
            keyValueTail--;
        }
        if (nextEqualsign == keyValueTail) {
            *status = U_ILLEGAL_ARGUMENT_ERROR; /* empty key value in passed-in locale */
            return 0;
        }

        rc = uprv_strcmp(keywordNameBuffer, localeKeywordNameBuffer);
        if(rc == 0) {
            /* Current entry matches the input keyword. Update the entry */
            if(keywordValueLen > 0) { /* updating a value */
                updatedKeysAndValues.append(keyValuePrefix, *status);
                keyValuePrefix = ';'; /* for any subsequent key-value pair */
                updatedKeysAndValues.append(keywordNameBuffer, keywordNameLen, *status);
                updatedKeysAndValues.append('=', *status);
                updatedKeysAndValues.append(keywordValueBuffer, keywordValueLen, *status);
            } /* else removing this entry, don't emit anything */
            handledInputKeyAndValue = true;
        } else {
           /* input keyword sorts earlier than current entry, add before current entry */
            if (rc < 0 && keywordValueLen > 0 && !handledInputKeyAndValue) {
                /* insert new entry at this location */
                updatedKeysAndValues.append(keyValuePrefix, *status);
                keyValuePrefix = ';'; /* for any subsequent key-value pair */
                updatedKeysAndValues.append(keywordNameBuffer, keywordNameLen, *status);
                updatedKeysAndValues.append('=', *status);
                updatedKeysAndValues.append(keywordValueBuffer, keywordValueLen, *status);
                handledInputKeyAndValue = true;
            }
            /* copy the current entry */
            updatedKeysAndValues.append(keyValuePrefix, *status);
            keyValuePrefix = ';'; /* for any subsequent key-value pair */
            updatedKeysAndValues.append(localeKeywordNameBuffer, keyValueLen, *status);
            updatedKeysAndValues.append('=', *status);
            updatedKeysAndValues.append(nextEqualsign, static_cast<int32_t>(keyValueTail-nextEqualsign), *status);
        }
        if (!nextSeparator && keywordValueLen > 0 && !handledInputKeyAndValue) {
            /* append new entry at the end, it sorts later than existing entries */
            updatedKeysAndValues.append(keyValuePrefix, *status);
            /* skip keyValuePrefix update, no subsequent key-value pair */
            updatedKeysAndValues.append(keywordNameBuffer, keywordNameLen, *status);
            updatedKeysAndValues.append('=', *status);
            updatedKeysAndValues.append(keywordValueBuffer, keywordValueLen, *status);
            handledInputKeyAndValue = true;
        }
        keywordStart = nextSeparator;
    } /* end loop searching */

    /* Any error from updatedKeysAndValues.append above would be internal and not due to
     * problems with the passed-in locale. So if we did encounter problems with the
     * passed-in locale above, those errors took precedence and overrode any error
     * status from updatedKeysAndValues.append, and also caused a return of 0. If there
     * are errors here they are from updatedKeysAndValues.append; they do cause an
     * error return but the passed-in locale is unmodified and the original bufLen is
     * returned.
     */
    if (!handledInputKeyAndValue || U_FAILURE(*status)) {
        /* if input key/value specified removal of a keyword not present in locale, or
         * there was an error in CharString.append, leave original locale alone. */
        U_ASSERT(*status != U_STRING_NOT_TERMINATED_WARNING);
        return bufLen;
    }

    // needLen = length of the part before '@'
    needLen = (int32_t)(startSearchHere - buffer);
    // Check to see can we fit the startSearchHere, if not, return
    // U_BUFFER_OVERFLOW_ERROR without copy updatedKeysAndValues into it.
    // We do this because this API function does not behave like most others:
    // It promises never to set a U_STRING_NOT_TERMINATED_WARNING.
    // When the contents fits but without the terminating NUL, in this case we need to not change
    // the buffer contents and return with a buffer overflow error.
    int32_t appendLength = updatedKeysAndValues.length();
    if (appendLength >= bufferCapacity - needLen) {
        *status = U_BUFFER_OVERFLOW_ERROR;
        return needLen + appendLength;
    }
    needLen += updatedKeysAndValues.extract(
                         startSearchHere, bufferCapacity - needLen, *status);
    U_ASSERT(*status != U_STRING_NOT_TERMINATED_WARNING);
    return needLen;
}

/* ### ID parsing implementation **************************************************/

#define _isPrefixLetter(a) ((a=='x')||(a=='X')||(a=='i')||(a=='I'))

/*returns true if one of the special prefixes is here (s=string)
  'x-' or 'i-' */
#define _isIDPrefix(s) (_isPrefixLetter(s[0])&&_isIDSeparator(s[1]))

/* Dot terminates it because of POSIX form  where dot precedes the codepage
 * except for variant
 */
#define _isTerminator(a)  ((a==0)||(a=='.')||(a=='@'))

/**
 * Lookup 'key' in the array 'list'.  The array 'list' should contain
 * a nullptr entry, followed by more entries, and a second nullptr entry.
 *
 * The 'list' param should be LANGUAGES, LANGUAGES_3, COUNTRIES, or
 * COUNTRIES_3.
 */
static int16_t _findIndex(const char* const* list, const char* key)
{
    const char* const* anchor = list;
    int32_t pass = 0;

    /* Make two passes through two nullptr-terminated arrays at 'list' */
    while (pass++ < 2) {
        while (*list) {
            if (uprv_strcmp(key, *list) == 0) {
                return (int16_t)(list - anchor);
            }
            list++;
        }
        ++list;     /* skip final nullptr *CWB*/
    }
    return -1;
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
CharString U_EXPORT2
ulocimp_getLanguage(const char *localeID,
                    const char **pEnd,
                    UErrorCode &status) {
    CharString result;

    if (uprv_stricmp(localeID, "root") == 0) {
        localeID += 4;
    } else if (uprv_strnicmp(localeID, "und", 3) == 0 &&
               (localeID[3] == '\0' ||
                localeID[3] == '-' ||
                localeID[3] == '_' ||
                localeID[3] == '@')) {
        localeID += 3;
    }

    /* if it starts with i- or x- then copy that prefix */
    if(_isIDPrefix(localeID)) {
        result.append((char)uprv_tolower(*localeID), status);
        result.append('-', status);
        localeID+=2;
    }

    /* copy the language as far as possible and count its length */
    while(!_isTerminator(*localeID) && !_isIDSeparator(*localeID)) {
        result.append((char)uprv_tolower(*localeID), status);
        localeID++;
    }

    if(result.length()==3) {
        /* convert 3 character code to 2 character code if possible *CWB*/
        int32_t offset = _findIndex(LANGUAGES_3, result.data());
        if(offset>=0) {
            result.clear();
            result.append(LANGUAGES[offset], status);
        }
    }

    if(pEnd!=nullptr) {
        *pEnd=localeID;
    }

    return result;
}

CharString U_EXPORT2
ulocimp_getScript(const char *localeID,
                  const char **pEnd,
                  UErrorCode &status) {
    CharString result;
    int32_t idLen = 0;

    if (pEnd != nullptr) {
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
        if (pEnd != nullptr) {
            *pEnd = localeID+idLen;
        }
        if (idLen >= 1) {
            result.append((char)uprv_toupper(*(localeID++)), status);
        }
        for (i = 1; i < idLen; i++) {
            result.append((char)uprv_tolower(*(localeID++)), status);
        }
    }

    return result;
}

CharString U_EXPORT2
ulocimp_getCountry(const char *localeID,
                   const char **pEnd,
                   UErrorCode &status) {
    CharString result;
    int32_t idLen=0;

    /* copy the country as far as possible and count its length */
    while(!_isTerminator(localeID[idLen]) && !_isIDSeparator(localeID[idLen])) {
        result.append((char)uprv_toupper(localeID[idLen]), status);
        idLen++;
    }

    /* the country should be either length 2 or 3 */
    if (idLen == 2 || idLen == 3) {
        /* convert 3 character code to 2 character code if possible *CWB*/
        if(idLen==3) {
            int32_t offset = _findIndex(COUNTRIES_3, result.data());
            if(offset>=0) {
                result.clear();
                result.append(COUNTRIES[offset], status);
            }
        }
        localeID+=idLen;
    } else {
        result.clear();
    }

    if(pEnd!=nullptr) {
        *pEnd=localeID;
    }

    return result;
}

/**
 * @param needSeparator if true, then add leading '_' if any variants
 * are added to 'variant'
 */
static void
_getVariant(const char *localeID,
            char prev,
            ByteSink& sink,
            UBool needSeparator) {
    UBool hasVariant = false;

    /* get one or more variant tags and separate them with '_' */
    if(_isIDSeparator(prev)) {
        /* get a variant string after a '-' or '_' */
        while(!_isTerminator(*localeID)) {
            if (needSeparator) {
                sink.Append("_", 1);
                needSeparator = false;
            }
            char c = (char)uprv_toupper(*localeID);
            if (c == '-') c = '_';
            sink.Append(&c, 1);
            hasVariant = true;
            localeID++;
        }
    }

    /* if there is no variant tag after a '-' or '_' then look for '@' */
    if(!hasVariant) {
        if(prev=='@') {
            /* keep localeID */
        } else if((localeID=locale_getKeywordsStart(localeID))!=nullptr) {
            ++localeID; /* point after the '@' */
        } else {
            return;
        }
        while(!_isTerminator(*localeID)) {
            if (needSeparator) {
                sink.Append("_", 1);
                needSeparator = false;
            }
            char c = (char)uprv_toupper(*localeID);
            if (c == '-' || c == ',') c = '_';
            sink.Append(&c, 1);
            localeID++;
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
        result = nullptr;
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
    nullptr,
    nullptr,
    uloc_kw_closeKeywords,
    uloc_kw_countKeywords,
    uenum_unextDefault,
    uloc_kw_nextKeyword,
    uloc_kw_resetKeywords
};

U_CAPI UEnumeration* U_EXPORT2
uloc_openKeywordList(const char *keywordList, int32_t keywordListSize, UErrorCode* status)
{
    LocalMemory<UKeywordsContext> myContext;
    LocalMemory<UEnumeration> result;

    if (U_FAILURE(*status)) {
        return nullptr;
    }
    myContext.adoptInstead(static_cast<UKeywordsContext *>(uprv_malloc(sizeof(UKeywordsContext))));
    result.adoptInstead(static_cast<UEnumeration *>(uprv_malloc(sizeof(UEnumeration))));
    if (myContext.isNull() || result.isNull()) {
        *status = U_MEMORY_ALLOCATION_ERROR;
        return nullptr;
    }
    uprv_memcpy(result.getAlias(), &gKeywordsEnum, sizeof(UEnumeration));
    myContext->keywords = static_cast<char *>(uprv_malloc(keywordListSize+1));
    if (myContext->keywords == nullptr) {
        *status = U_MEMORY_ALLOCATION_ERROR;
        return nullptr;
    }
    uprv_memcpy(myContext->keywords, keywordList, keywordListSize);
    myContext->keywords[keywordListSize] = 0;
    myContext->current = myContext->keywords;
    result->context = myContext.orphan();
    return result.orphan();
}

U_CAPI UEnumeration* U_EXPORT2
uloc_openKeywords(const char* localeID,
                        UErrorCode* status)
{
    CharString tempBuffer;
    const char* tmpLocaleID;

    if(status==nullptr || U_FAILURE(*status)) {
        return 0;
    }

    if (_hasBCP47Extension(localeID)) {
        CharStringByteSink sink(&tempBuffer);
        ulocimp_forLanguageTag(localeID, -1, sink, nullptr, status);
        tmpLocaleID = U_SUCCESS(*status) && !tempBuffer.isEmpty() ? tempBuffer.data() : localeID;
    } else {
        if (localeID==nullptr) {
            localeID=uloc_getDefault();
        }
        tmpLocaleID=localeID;
    }

    /* Skip the language */
    ulocimp_getLanguage(tmpLocaleID, &tmpLocaleID, *status);
    if (U_FAILURE(*status)) {
        return 0;
    }

    if(_isIDSeparator(*tmpLocaleID)) {
        const char *scriptID;
        /* Skip the script if available */
        ulocimp_getScript(tmpLocaleID+1, &scriptID, *status);
        if (U_FAILURE(*status)) {
            return 0;
        }
        if(scriptID != tmpLocaleID+1) {
            /* Found optional script */
            tmpLocaleID = scriptID;
        }
        /* Skip the Country */
        if (_isIDSeparator(*tmpLocaleID)) {
            ulocimp_getCountry(tmpLocaleID+1, &tmpLocaleID, *status);
            if (U_FAILURE(*status)) {
                return 0;
            }
        }
    }

    /* keywords are located after '@' */
    if((tmpLocaleID = locale_getKeywordsStart(tmpLocaleID)) != nullptr) {
        CharString keywords;
        CharStringByteSink sink(&keywords);
        ulocimp_getKeywords(tmpLocaleID+1, '@', sink, false, status);
        if (U_FAILURE(*status)) {
            return nullptr;
        }
        return uloc_openKeywordList(keywords.data(), keywords.length(), status);
    }
    return nullptr;
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
static void
_canonicalize(const char* localeID,
              ByteSink& sink,
              uint32_t options,
              UErrorCode* err) {
    if (U_FAILURE(*err)) {
        return;
    }

    int32_t j, fieldCount=0, scriptSize=0, variantSize=0;
    CharString tempBuffer;  // if localeID has a BCP47 extension, tmpLocaleID points to this
    CharString localeIDWithHyphens;  // if localeID has a BPC47 extension and have _, tmpLocaleID points to this
    const char* origLocaleID;
    const char* tmpLocaleID;
    const char* keywordAssign = nullptr;
    const char* separatorIndicator = nullptr;

    if (_hasBCP47Extension(localeID)) {
        const char* localeIDPtr = localeID;

        // convert all underbars to hyphens, unless the "BCP47 extension" comes at the beginning of the string
        if (uprv_strchr(localeID, '_') != nullptr && localeID[1] != '-' && localeID[1] != '_') {
            localeIDWithHyphens.append(localeID, -1, *err);
            if (U_SUCCESS(*err)) {
                for (char* p = localeIDWithHyphens.data(); *p != '\0'; ++p) {
                    if (*p == '_') {
                        *p = '-';
                    }
                }
                localeIDPtr = localeIDWithHyphens.data();
            }
        }

        CharStringByteSink tempSink(&tempBuffer);
        ulocimp_forLanguageTag(localeIDPtr, -1, tempSink, nullptr, err);
        tmpLocaleID = U_SUCCESS(*err) && !tempBuffer.isEmpty() ? tempBuffer.data() : localeIDPtr;
    } else {
        if (localeID==nullptr) {
           localeID=uloc_getDefault();
        }
        tmpLocaleID=localeID;
    }

    origLocaleID=tmpLocaleID;

    /* get all pieces, one after another, and separate with '_' */
    CharString tag = ulocimp_getLanguage(tmpLocaleID, &tmpLocaleID, *err);

    if (tag.length() == I_DEFAULT_LENGTH &&
            uprv_strncmp(origLocaleID, i_default, I_DEFAULT_LENGTH) == 0) {
        tag.clear();
        tag.append(uloc_getDefault(), *err);
    } else if(_isIDSeparator(*tmpLocaleID)) {
        const char *scriptID;

        ++fieldCount;
        tag.append('_', *err);

        CharString script = ulocimp_getScript(tmpLocaleID+1, &scriptID, *err);
        tag.append(script, *err);
        scriptSize = script.length();
        if(scriptSize > 0) {
            /* Found optional script */
            tmpLocaleID = scriptID;
            ++fieldCount;
            if (_isIDSeparator(*tmpLocaleID)) {
                /* If there is something else, then we add the _ */
                tag.append('_', *err);
            }
        }

        if (_isIDSeparator(*tmpLocaleID)) {
            const char *cntryID;

            CharString country = ulocimp_getCountry(tmpLocaleID+1, &cntryID, *err);
            tag.append(country, *err);
            if (!country.isEmpty()) {
                /* Found optional country */
                tmpLocaleID = cntryID;
            }
            if(_isIDSeparator(*tmpLocaleID)) {
                /* If there is something else, then we add the _  if we found country before. */
                if (!_isIDSeparator(*(tmpLocaleID+1))) {
                    ++fieldCount;
                    tag.append('_', *err);
                }

                variantSize = -tag.length();
                {
                    CharStringByteSink s(&tag);
                    _getVariant(tmpLocaleID+1, *tmpLocaleID, s, false);
                }
                variantSize += tag.length();
                if (variantSize > 0) {
                    tmpLocaleID += variantSize + 1; /* skip '_' and variant */
                }
            }
        }
    }

    /* Copy POSIX-style charset specifier, if any [mr.utf8] */
    if (!OPTION_SET(options, _ULOC_CANONICALIZE) && *tmpLocaleID == '.') {
        UBool done = false;
        do {
            char c = *tmpLocaleID;
            switch (c) {
            case 0:
            case '@':
                done = true;
                break;
            default:
                tag.append(c, *err);
                ++tmpLocaleID;
                break;
            }
        } while (!done);
    }

    /* Scan ahead to next '@' and determine if it is followed by '=' and/or ';'
       After this, tmpLocaleID either points to '@' or is nullptr */
    if ((tmpLocaleID=locale_getKeywordsStart(tmpLocaleID))!=nullptr) {
        keywordAssign = uprv_strchr(tmpLocaleID, '=');
        separatorIndicator = uprv_strchr(tmpLocaleID, ';');
    }

    /* Copy POSIX-style variant, if any [mr@FOO] */
    if (!OPTION_SET(options, _ULOC_CANONICALIZE) &&
        tmpLocaleID != nullptr && keywordAssign == nullptr) {
        for (;;) {
            char c = *tmpLocaleID;
            if (c == 0) {
                break;
            }
            tag.append(c, *err);
            ++tmpLocaleID;
        }
    }

    if (OPTION_SET(options, _ULOC_CANONICALIZE)) {
        /* Handle @FOO variant if @ is present and not followed by = */
        if (tmpLocaleID!=nullptr && keywordAssign==nullptr) {
            /* Add missing '_' if needed */
            if (fieldCount < 2 || (fieldCount < 3 && scriptSize > 0)) {
                do {
                    tag.append('_', *err);
                    ++fieldCount;
                } while(fieldCount<2);
            }

            int32_t posixVariantSize = -tag.length();
            {
                CharStringByteSink s(&tag);
                _getVariant(tmpLocaleID+1, '@', s, (UBool)(variantSize > 0));
            }
            posixVariantSize += tag.length();
            if (posixVariantSize > 0) {
                variantSize += posixVariantSize;
            }
        }

        /* Look up the ID in the canonicalization map */
        for (j=0; j<UPRV_LENGTHOF(CANONICALIZE_MAP); j++) {
            StringPiece id(CANONICALIZE_MAP[j].id);
            if (tag == id) {
                if (id.empty() && tmpLocaleID != nullptr) {
                    break; /* Don't remap "" if keywords present */
                }
                tag.clear();
                tag.append(CANONICALIZE_MAP[j].canonicalID, *err);
                break;
            }
        }
    }

    sink.Append(tag.data(), tag.length());

    if (!OPTION_SET(options, _ULOC_STRIP_KEYWORDS)) {
        if (tmpLocaleID!=nullptr && keywordAssign!=nullptr &&
            (!separatorIndicator || separatorIndicator > keywordAssign)) {
            sink.Append("@", 1);
            ++fieldCount;
            ulocimp_getKeywords(tmpLocaleID+1, '@', sink, true, err);
        }
    }
}

/* ### ID parsing API **************************************************/

U_CAPI int32_t  U_EXPORT2
uloc_getParent(const char*    localeID,
               char* parent,
               int32_t parentCapacity,
               UErrorCode* err)
{
    if (U_FAILURE(*err)) {
        return 0;
    }

    CheckedArrayByteSink sink(parent, parentCapacity);
    ulocimp_getParent(localeID, sink, err);

    int32_t reslen = sink.NumberOfBytesAppended();

    if (U_FAILURE(*err)) {
        return reslen;
    }

    if (sink.Overflowed()) {
        *err = U_BUFFER_OVERFLOW_ERROR;
    } else {
        u_terminateChars(parent, parentCapacity, reslen, err);
    }

    return reslen;
}

U_CAPI void U_EXPORT2
ulocimp_getParent(const char* localeID,
                  icu::ByteSink& sink,
                  UErrorCode* err)
{
    const char *lastUnderscore;
    int32_t i;

    if (U_FAILURE(*err))
        return;

    if (localeID == nullptr)
        localeID = uloc_getDefault();

    lastUnderscore=uprv_strrchr(localeID, '_');
    if(lastUnderscore!=nullptr) {
        i=(int32_t)(lastUnderscore-localeID);
    } else {
        i=0;
    }

    if (i > 0) {
        if (uprv_strnicmp(localeID, "und_", 4) == 0) {
            localeID += 3;
            i -= 3;
        }
        sink.Append(localeID, i);
    }
}

U_CAPI int32_t U_EXPORT2
uloc_getLanguage(const char*    localeID,
         char* language,
         int32_t languageCapacity,
         UErrorCode* err)
{
    /* uloc_getLanguage will return a 2 character iso-639 code if one exists. *CWB*/

    if (err==nullptr || U_FAILURE(*err)) {
        return 0;
    }

    if(localeID==nullptr) {
        localeID=uloc_getDefault();
    }

    return ulocimp_getLanguage(localeID, nullptr, *err).extract(language, languageCapacity, *err);
}

U_CAPI int32_t U_EXPORT2
uloc_getScript(const char*    localeID,
         char* script,
         int32_t scriptCapacity,
         UErrorCode* err)
{
    if(err==nullptr || U_FAILURE(*err)) {
        return 0;
    }

    if(localeID==nullptr) {
        localeID=uloc_getDefault();
    }

    /* skip the language */
    ulocimp_getLanguage(localeID, &localeID, *err);
    if (U_FAILURE(*err)) {
        return 0;
    }

    if(_isIDSeparator(*localeID)) {
        return ulocimp_getScript(localeID+1, nullptr, *err).extract(script, scriptCapacity, *err);
    }
    return u_terminateChars(script, scriptCapacity, 0, err);
}

U_CAPI int32_t  U_EXPORT2
uloc_getCountry(const char* localeID,
            char* country,
            int32_t countryCapacity,
            UErrorCode* err)
{
    if(err==nullptr || U_FAILURE(*err)) {
        return 0;
    }

    if(localeID==nullptr) {
        localeID=uloc_getDefault();
    }

    /* Skip the language */
    ulocimp_getLanguage(localeID, &localeID, *err);
    if (U_FAILURE(*err)) {
        return 0;
    }

    if(_isIDSeparator(*localeID)) {
        const char *scriptID;
        /* Skip the script if available */
        ulocimp_getScript(localeID+1, &scriptID, *err);
        if (U_FAILURE(*err)) {
            return 0;
        }
        if(scriptID != localeID+1) {
            /* Found optional script */
            localeID = scriptID;
        }
        if(_isIDSeparator(*localeID)) {
            return ulocimp_getCountry(localeID+1, nullptr, *err).extract(country, countryCapacity, *err);
        }
    }
    return u_terminateChars(country, countryCapacity, 0, err);
}

U_CAPI int32_t  U_EXPORT2
uloc_getVariant(const char* localeID,
                char* variant,
                int32_t variantCapacity,
                UErrorCode* err)
{
    CharString tempBuffer;
    const char* tmpLocaleID;
    int32_t i=0;

    if(err==nullptr || U_FAILURE(*err)) {
        return 0;
    }

    if (_hasBCP47Extension(localeID)) {
        CharStringByteSink sink(&tempBuffer);
        ulocimp_forLanguageTag(localeID, -1, sink, nullptr, err);
        tmpLocaleID = U_SUCCESS(*err) && !tempBuffer.isEmpty() ? tempBuffer.data() : localeID;
    } else {
        if (localeID==nullptr) {
           localeID=uloc_getDefault();
        }
        tmpLocaleID=localeID;
    }

    /* Skip the language */
    ulocimp_getLanguage(tmpLocaleID, &tmpLocaleID, *err);
    if (U_FAILURE(*err)) {
        return 0;
    }

    if(_isIDSeparator(*tmpLocaleID)) {
        const char *scriptID;
        /* Skip the script if available */
        ulocimp_getScript(tmpLocaleID+1, &scriptID, *err);
        if (U_FAILURE(*err)) {
            return 0;
        }
        if(scriptID != tmpLocaleID+1) {
            /* Found optional script */
            tmpLocaleID = scriptID;
        }
        /* Skip the Country */
        if (_isIDSeparator(*tmpLocaleID)) {
            const char *cntryID;
            ulocimp_getCountry(tmpLocaleID+1, &cntryID, *err);
            if (U_FAILURE(*err)) {
                return 0;
            }
            if (cntryID != tmpLocaleID+1) {
                /* Found optional country */
                tmpLocaleID = cntryID;
            }
            if(_isIDSeparator(*tmpLocaleID)) {
                /* If there was no country ID, skip a possible extra IDSeparator */
                if (tmpLocaleID != cntryID && _isIDSeparator(tmpLocaleID[1])) {
                    tmpLocaleID++;
                }

                CheckedArrayByteSink sink(variant, variantCapacity);
                _getVariant(tmpLocaleID+1, *tmpLocaleID, sink, false);

                i = sink.NumberOfBytesAppended();

                if (U_FAILURE(*err)) {
                    return i;
                }

                if (sink.Overflowed()) {
                    *err = U_BUFFER_OVERFLOW_ERROR;
                    return i;
                }
            }
        }
    }

    return u_terminateChars(variant, variantCapacity, i, err);
}

U_CAPI int32_t  U_EXPORT2
uloc_getName(const char* localeID,
             char* name,
             int32_t nameCapacity,
             UErrorCode* err)
{
    if (U_FAILURE(*err)) {
        return 0;
    }

    CheckedArrayByteSink sink(name, nameCapacity);
    ulocimp_getName(localeID, sink, err);

    int32_t reslen = sink.NumberOfBytesAppended();

    if (U_FAILURE(*err)) {
        return reslen;
    }

    if (sink.Overflowed()) {
        *err = U_BUFFER_OVERFLOW_ERROR;
    } else {
        u_terminateChars(name, nameCapacity, reslen, err);
    }

    return reslen;
}

U_CAPI void U_EXPORT2
ulocimp_getName(const char* localeID,
                ByteSink& sink,
                UErrorCode* err)
{
    _canonicalize(localeID, sink, 0, err);
}

U_CAPI int32_t  U_EXPORT2
uloc_getBaseName(const char* localeID,
                 char* name,
                 int32_t nameCapacity,
                 UErrorCode* err)
{
    if (U_FAILURE(*err)) {
        return 0;
    }

    CheckedArrayByteSink sink(name, nameCapacity);
    ulocimp_getBaseName(localeID, sink, err);

    int32_t reslen = sink.NumberOfBytesAppended();

    if (U_FAILURE(*err)) {
        return reslen;
    }

    if (sink.Overflowed()) {
        *err = U_BUFFER_OVERFLOW_ERROR;
    } else {
        u_terminateChars(name, nameCapacity, reslen, err);
    }

    return reslen;
}

U_CAPI void U_EXPORT2
ulocimp_getBaseName(const char* localeID,
                    ByteSink& sink,
                    UErrorCode* err)
{
    _canonicalize(localeID, sink, _ULOC_STRIP_KEYWORDS, err);
}

U_CAPI int32_t  U_EXPORT2
uloc_canonicalize(const char* localeID,
                  char* name,
                  int32_t nameCapacity,
                  UErrorCode* err)
{
    if (U_FAILURE(*err)) {
        return 0;
    }

    CheckedArrayByteSink sink(name, nameCapacity);
    ulocimp_canonicalize(localeID, sink, err);

    int32_t reslen = sink.NumberOfBytesAppended();

    if (U_FAILURE(*err)) {
        return reslen;
    }

    if (sink.Overflowed()) {
        *err = U_BUFFER_OVERFLOW_ERROR;
    } else {
        u_terminateChars(name, nameCapacity, reslen, err);
    }

    return reslen;
}

U_CAPI void U_EXPORT2
ulocimp_canonicalize(const char* localeID,
                     ByteSink& sink,
                     UErrorCode* err)
{
    _canonicalize(localeID, sink, _ULOC_CANONICALIZE, err);
}

U_CAPI const char*  U_EXPORT2
uloc_getISO3Language(const char* localeID)
{
    int16_t offset;
    char lang[ULOC_LANG_CAPACITY];
    UErrorCode err = U_ZERO_ERROR;

    if (localeID == nullptr)
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

    if (localeID == nullptr)
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
    uint32_t   lcid = 0;

    /* Check for incomplete id. */
    if (!localeID || uprv_strlen(localeID) < 2) {
        return 0;
    }

    // First, attempt Windows platform lookup if available, but fall
    // through to catch any special cases (ICU vs Windows name differences).
    lcid = uprv_convertToLCIDPlatform(localeID, &status);
    if (U_FAILURE(status)) {
        return 0;
    }
    if (lcid > 0) {
        // Windows found an LCID, return that
        return lcid;
    }

    uloc_getLanguage(localeID, langID, sizeof(langID), &status);
    if (U_FAILURE(status) || status == U_STRING_NOT_TERMINATED_WARNING) {
        return 0;
    }

    if (uprv_strchr(localeID, '@')) {
        // uprv_convertToLCID does not support keywords other than collation.
        // Remove all keywords except collation.
        int32_t len;
        char tmpLocaleID[ULOC_FULLNAME_CAPACITY];

        CharString collVal;
        {
            CharStringByteSink sink(&collVal);
            ulocimp_getKeywordValue(localeID, "collation", sink, &status);
        }

        if (U_SUCCESS(status) && !collVal.isEmpty()) {
            len = uloc_getBaseName(localeID, tmpLocaleID,
                UPRV_LENGTHOF(tmpLocaleID) - 1, &status);

            if (U_SUCCESS(status) && len > 0) {
                tmpLocaleID[len] = 0;

                len = uloc_setKeywordValue("collation", collVal.data(), tmpLocaleID,
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

U_CAPI const char* U_EXPORT2
uloc_toUnicodeLocaleKey(const char* keyword)
{
    const char* bcpKey = ulocimp_toBcpKey(keyword);
    if (bcpKey == nullptr && ultag_isUnicodeLocaleKey(keyword, -1)) {
        // unknown keyword, but syntax is fine..
        return keyword;
    }
    return bcpKey;
}

U_CAPI const char* U_EXPORT2
uloc_toUnicodeLocaleType(const char* keyword, const char* value)
{
    const char* bcpType = ulocimp_toBcpType(keyword, value, nullptr, nullptr);
    if (bcpType == nullptr && ultag_isUnicodeLocaleType(value, -1)) {
        // unknown keyword, but syntax is fine..
        return value;
    }
    return bcpType;
}

static UBool
isWellFormedLegacyKey(const char* legacyKey)
{
    const char* p = legacyKey;
    while (*p) {
        if (!UPRV_ISALPHANUM(*p)) {
            return false;
        }
        p++;
    }
    return true;
}

static UBool
isWellFormedLegacyType(const char* legacyType)
{
    const char* p = legacyType;
    int32_t alphaNumLen = 0;
    while (*p) {
        if (*p == '_' || *p == '/' || *p == '-') {
            if (alphaNumLen == 0) {
                return false;
            }
            alphaNumLen = 0;
        } else if (UPRV_ISALPHANUM(*p)) {
            alphaNumLen++;
        } else {
            return false;
        }
        p++;
    }
    return (alphaNumLen != 0);
}

U_CAPI const char* U_EXPORT2
uloc_toLegacyKey(const char* keyword)
{
    const char* legacyKey = ulocimp_toLegacyKey(keyword);
    if (legacyKey == nullptr) {
        // Checks if the specified locale key is well-formed with the legacy locale syntax.
        //
        // Note:
        //  LDML/CLDR provides some definition of keyword syntax in
        //  * http://www.unicode.org/reports/tr35/#Unicode_locale_identifier and
        //  * http://www.unicode.org/reports/tr35/#Old_Locale_Extension_Syntax
        //  Keys can only consist of [0-9a-zA-Z].
        if (isWellFormedLegacyKey(keyword)) {
            return keyword;
        }
    }
    return legacyKey;
}

U_CAPI const char* U_EXPORT2
uloc_toLegacyType(const char* keyword, const char* value)
{
    const char* legacyType = ulocimp_toLegacyType(keyword, value, nullptr, nullptr);
    if (legacyType == nullptr) {
        // Checks if the specified locale type is well-formed with the legacy locale syntax.
        //
        // Note:
        //  LDML/CLDR provides some definition of keyword syntax in
        //  * http://www.unicode.org/reports/tr35/#Unicode_locale_identifier and
        //  * http://www.unicode.org/reports/tr35/#Old_Locale_Extension_Syntax
        //  Values (types) can only consist of [0-9a-zA-Z], plus for legacy values
        //  we allow [/_-+] in the middle (e.g. "Etc/GMT+1", "Asia/Tel_Aviv")
        if (isWellFormedLegacyType(value)) {
            return value;
        }
    }
    return legacyType;
}

/*eof*/
