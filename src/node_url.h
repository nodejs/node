#ifndef SRC_NODE_URL_H_
#define SRC_NODE_URL_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "node.h"
#include <string>

namespace node {
namespace url {

#define BIT_AT(a, i)                                                          \
  (!!((unsigned int) (a)[(unsigned int) (i) >> 3] &                           \
  (1 << ((unsigned int) (i) & 7))))
#define TAB_AND_NEWLINE(ch)                                                   \
  (ch == 0x09 || ch == 0x0a || ch == 0x0d)
#define ASCII_DIGIT(ch)                                                       \
  (ch >= 0x30 && ch <= 0x39)
#define ASCII_HEX_DIGIT(ch)                                                   \
  (ASCII_DIGIT(ch) || (ch >= 0x41 && ch <= 0x46) || (ch >= 0x61 && ch <= 0x66))
#define ASCII_ALPHA(ch)                                                       \
  ((ch >= 0x41 && ch <= 0x5a) || (ch >= 0x61 && ch <= 0x7a))
#define ASCII_ALPHANUMERIC(ch)                                                \
  (ASCII_DIGIT(ch) || ASCII_ALPHA(ch))
#define TO_LOWER(ch)                                                          \
  (ASCII_ALPHA(ch) ? (ch | 0x20) : ch)
#define SCHEME_CHAR(ch)                                                       \
  (ASCII_ALPHANUMERIC(ch) || ch == '+' || ch == '-' || ch == '.')
#define WINDOWS_DRIVE_LETTER(ch, next)                                        \
  (ASCII_ALPHA(ch) && (next == ':' || next == '|'))
#define NORMALIZED_WINDOWS_DRIVE_LETTER(str)                                  \
  (str.length() == 2 &&                                                       \
  ASCII_ALPHA(str[0]) &&                                                      \
  str[1] == ':')

static const char* hex[256] = {
  "%00", "%01", "%02", "%03", "%04", "%05", "%06", "%07",
  "%08", "%09", "%0A", "%0B", "%0C", "%0D", "%0E", "%0F",
  "%10", "%11", "%12", "%13", "%14", "%15", "%16", "%17",
  "%18", "%19", "%1A", "%1B", "%1C", "%1D", "%1E", "%1F",
  "%20", "%21", "%22", "%23", "%24", "%25", "%26", "%27",
  "%28", "%29", "%2A", "%2B", "%2C", "%2D", "%2E", "%2F",
  "%30", "%31", "%32", "%33", "%34", "%35", "%36", "%37",
  "%38", "%39", "%3A", "%3B", "%3C", "%3D", "%3E", "%3F",
  "%40", "%41", "%42", "%43", "%44", "%45", "%46", "%47",
  "%48", "%49", "%4A", "%4B", "%4C", "%4D", "%4E", "%4F",
  "%50", "%51", "%52", "%53", "%54", "%55", "%56", "%57",
  "%58", "%59", "%5A", "%5B", "%5C", "%5D", "%5E", "%5F",
  "%60", "%61", "%62", "%63", "%64", "%65", "%66", "%67",
  "%68", "%69", "%6A", "%6B", "%6C", "%6D", "%6E", "%6F",
  "%70", "%71", "%72", "%73", "%74", "%75", "%76", "%77",
  "%78", "%79", "%7A", "%7B", "%7C", "%7D", "%7E", "%7F",
  "%80", "%81", "%82", "%83", "%84", "%85", "%86", "%87",
  "%88", "%89", "%8A", "%8B", "%8C", "%8D", "%8E", "%8F",
  "%90", "%91", "%92", "%93", "%94", "%95", "%96", "%97",
  "%98", "%99", "%9A", "%9B", "%9C", "%9D", "%9E", "%9F",
  "%A0", "%A1", "%A2", "%A3", "%A4", "%A5", "%A6", "%A7",
  "%A8", "%A9", "%AA", "%AB", "%AC", "%AD", "%AE", "%AF",
  "%B0", "%B1", "%B2", "%B3", "%B4", "%B5", "%B6", "%B7",
  "%B8", "%B9", "%BA", "%BB", "%BC", "%BD", "%BE", "%BF",
  "%C0", "%C1", "%C2", "%C3", "%C4", "%C5", "%C6", "%C7",
  "%C8", "%C9", "%CA", "%CB", "%CC", "%CD", "%CE", "%CF",
  "%D0", "%D1", "%D2", "%D3", "%D4", "%D5", "%D6", "%D7",
  "%D8", "%D9", "%DA", "%DB", "%DC", "%DD", "%DE", "%DF",
  "%E0", "%E1", "%E2", "%E3", "%E4", "%E5", "%E6", "%E7",
  "%E8", "%E9", "%EA", "%EB", "%EC", "%ED", "%EE", "%EF",
  "%F0", "%F1", "%F2", "%F3", "%F4", "%F5", "%F6", "%F7",
  "%F8", "%F9", "%FA", "%FB", "%FC", "%FD", "%FE", "%FF"
};

static const uint8_t SIMPLE_ENCODE_SET[32] = {
  // 00     01     02     03     04     05     06     07
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // 08     09     0A     0B     0C     0D     0E     0F
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // 10     11     12     13     14     15     16     17
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // 18     19     1A     1B     1C     1D     1E     1F
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // 20     21     22     23     24     25     26     27
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
  // 28     29     2A     2B     2C     2D     2E     2F
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
  // 30     31     32     33     34     35     36     37
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
  // 38     39     3A     3B     3C     3D     3E     3F
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
  // 40     41     42     43     44     45     46     47
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
  // 48     49     4A     4B     4C     4D     4E     4F
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
  // 50     51     52     53     54     55     56     57
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
  // 58     59     5A     5B     5C     5D     5E     5F
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
  // 60     61     62     63     64     65     66     67
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
  // 68     69     6A     6B     6C     6D     6E     6F
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
  // 70     71     72     73     74     75     76     77
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
  // 78     79     7A     7B     7C     7D     7E     7F
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x80,
  // 80     81     82     83     84     85     86     87
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // 88     89     8A     8B     8C     8D     8E     8F
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // 90     91     92     93     94     95     96     97
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // 98     99     9A     9B     9C     9D     9E     9F
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // A0     A1     A2     A3     A4     A5     A6     A7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // A8     A9     AA     AB     AC     AD     AE     AF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // B0     B1     B2     B3     B4     B5     B6     B7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // B8     B9     BA     BB     BC     BD     BE     BF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // C0     C1     C2     C3     C4     C5     C6     C7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // C8     C9     CA     CB     CC     CD     CE     CF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // D0     D1     D2     D3     D4     D5     D6     D7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // D8     D9     DA     DB     DC     DD     DE     DF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // E0     E1     E2     E3     E4     E5     E6     E7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // E8     E9     EA     EB     EC     ED     EE     EF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // F0     F1     F2     F3     F4     F5     F6     F7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // F8     F9     FA     FB     FC     FD     FE     FF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80
};

static const uint8_t DEFAULT_ENCODE_SET[32] = {
  // 00     01     02     03     04     05     06     07
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // 08     09     0A     0B     0C     0D     0E     0F
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // 10     11     12     13     14     15     16     17
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // 18     19     1A     1B     1C     1D     1E     1F
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // 20     21     22     23     24     25     26     27
    0x01 | 0x00 | 0x04 | 0x08 | 0x00 | 0x00 | 0x00 | 0x00,
  // 28     29     2A     2B     2C     2D     2E     2F
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
  // 30     31     32     33     34     35     36     37
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
  // 38     39     3A     3B     3C     3D     3E     3F
    0x00 | 0x00 | 0x00 | 0x00 | 0x10 | 0x00 | 0x40 | 0x80,
  // 40     41     42     43     44     45     46     47
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
  // 48     49     4A     4B     4C     4D     4E     4F
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
  // 50     51     52     53     54     55     56     57
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
  // 58     59     5A     5B     5C     5D     5E     5F
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
  // 60     61     62     63     64     65     66     67
    0x01 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
  // 68     69     6A     6B     6C     6D     6E     6F
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
  // 70     71     72     73     74     75     76     77
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
  // 78     79     7A     7B     7C     7D     7E     7F
    0x00 | 0x00 | 0x00 | 0x08 | 0x00 | 0x20 | 0x00 | 0x80,
  // 80     81     82     83     84     85     86     87
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // 88     89     8A     8B     8C     8D     8E     8F
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // 90     91     92     93     94     95     96     97
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // 98     99     9A     9B     9C     9D     9E     9F
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // A0     A1     A2     A3     A4     A5     A6     A7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // A8     A9     AA     AB     AC     AD     AE     AF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // B0     B1     B2     B3     B4     B5     B6     B7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // B8     B9     BA     BB     BC     BD     BE     BF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // C0     C1     C2     C3     C4     C5     C6     C7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // C8     C9     CA     CB     CC     CD     CE     CF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // D0     D1     D2     D3     D4     D5     D6     D7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // D8     D9     DA     DB     DC     DD     DE     DF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // E0     E1     E2     E3     E4     E5     E6     E7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // E8     E9     EA     EB     EC     ED     EE     EF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // F0     F1     F2     F3     F4     F5     F6     F7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // F8     F9     FA     FB     FC     FD     FE     FF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80
};

static const uint8_t USERINFO_ENCODE_SET[32] = {
  // 00     01     02     03     04     05     06     07
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // 08     09     0A     0B     0C     0D     0E     0F
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // 10     11     12     13     14     15     16     17
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // 18     19     1A     1B     1C     1D     1E     1F
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // 20     21     22     23     24     25     26     27
    0x01 | 0x00 | 0x04 | 0x08 | 0x00 | 0x00 | 0x00 | 0x00,
  // 28     29     2A     2B     2C     2D     2E     2F
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x80,
  // 30     31     32     33     34     35     36     37
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
  // 38     39     3A     3B     3C     3D     3E     3F
    0x00 | 0x00 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // 40     41     42     43     44     45     46     47
    0x01 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
  // 48     49     4A     4B     4C     4D     4E     4F
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
  // 50     51     52     53     54     55     56     57
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
  // 58     59     5A     5B     5C     5D     5E     5F
    0x00 | 0x00 | 0x00 | 0x08 | 0x10 | 0x20 | 0x40 | 0x00,
  // 60     61     62     63     64     65     66     67
    0x01 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
  // 68     69     6A     6B     6C     6D     6E     6F
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
  // 70     71     72     73     74     75     76     77
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
  // 78     79     7A     7B     7C     7D     7E     7F
    0x00 | 0x00 | 0x00 | 0x08 | 0x10 | 0x20 | 0x00 | 0x80,
  // 80     81     82     83     84     85     86     87
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // 88     89     8A     8B     8C     8D     8E     8F
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // 90     91     92     93     94     95     96     97
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // 98     99     9A     9B     9C     9D     9E     9F
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // A0     A1     A2     A3     A4     A5     A6     A7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // A8     A9     AA     AB     AC     AD     AE     AF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // B0     B1     B2     B3     B4     B5     B6     B7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // B8     B9     BA     BB     BC     BD     BE     BF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // C0     C1     C2     C3     C4     C5     C6     C7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // C8     C9     CA     CB     CC     CD     CE     CF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // D0     D1     D2     D3     D4     D5     D6     D7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // D8     D9     DA     DB     DC     DD     DE     DF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // E0     E1     E2     E3     E4     E5     E6     E7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // E8     E9     EA     EB     EC     ED     EE     EF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // F0     F1     F2     F3     F4     F5     F6     F7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // F8     F9     FA     FB     FC     FD     FE     FF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80
};

static const uint8_t QUERY_ENCODE_SET[32] = {
  // 00     01     02     03     04     05     06     07
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // 08     09     0A     0B     0C     0D     0E     0F
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // 10     11     12     13     14     15     16     17
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // 18     19     1A     1B     1C     1D     1E     1F
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // 20     21     22     23     24     25     26     27
    0x01 | 0x00 | 0x04 | 0x08 | 0x00 | 0x00 | 0x00 | 0x00,
  // 28     29     2A     2B     2C     2D     2E     2F
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
  // 30     31     32     33     34     35     36     37
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
  // 38     39     3A     3B     3C     3D     3E     3F
    0x00 | 0x00 | 0x00 | 0x00 | 0x10 | 0x00 | 0x40 | 0x00,
  // 40     41     42     43     44     45     46     47
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
  // 48     49     4A     4B     4C     4D     4E     4F
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
  // 50     51     52     53     54     55     56     57
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
  // 58     59     5A     5B     5C     5D     5E     5F
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
  // 60     61     62     63     64     65     66     67
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
  // 68     69     6A     6B     6C     6D     6E     6F
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
  // 70     71     72     73     74     75     76     77
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
  // 78     79     7A     7B     7C     7D     7E     7F
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x80,
  // 80     81     82     83     84     85     86     87
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // 88     89     8A     8B     8C     8D     8E     8F
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // 90     91     92     93     94     95     96     97
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // 98     99     9A     9B     9C     9D     9E     9F
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // A0     A1     A2     A3     A4     A5     A6     A7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // A8     A9     AA     AB     AC     AD     AE     AF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // B0     B1     B2     B3     B4     B5     B6     B7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // B8     B9     BA     BB     BC     BD     BE     BF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // C0     C1     C2     C3     C4     C5     C6     C7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // C8     C9     CA     CB     CC     CD     CE     CF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // D0     D1     D2     D3     D4     D5     D6     D7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // D8     D9     DA     DB     DC     DD     DE     DF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // E0     E1     E2     E3     E4     E5     E6     E7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // E8     E9     EA     EB     EC     ED     EE     EF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // F0     F1     F2     F3     F4     F5     F6     F7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // F8     F9     FA     FB     FC     FD     FE     FF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80
};

// Must return true if the character is to be percent-encoded
typedef bool (*must_escape_cb)(const unsigned char ch);

// Appends ch to str. If test(ch) returns true, the ch will
// be percent-encoded then appended.
static inline void AppendOrEscape(std::string* str,
                                  const unsigned char ch,
                                  must_escape_cb test) {
  if (test(ch))
    *str += hex[ch];
  else
    *str += ch;
}

static inline bool SimpleEncodeSet(const unsigned char ch) {
  return BIT_AT(SIMPLE_ENCODE_SET, ch);
}

static inline bool DefaultEncodeSet(const unsigned char ch) {
  return BIT_AT(DEFAULT_ENCODE_SET, ch);
}

static inline bool UserinfoEncodeSet(const unsigned char ch) {
  return BIT_AT(USERINFO_ENCODE_SET, ch);
}

static inline bool QueryEncodeSet(const unsigned char ch) {
  return BIT_AT(QUERY_ENCODE_SET, ch);
}

static inline unsigned hex2bin(const char ch) {
  if (ch >= '0' && ch <= '9')
    return ch - '0';
  if (ch >= 'A' && ch <= 'F')
    return 10 + (ch - 'A');
  if (ch >= 'a' && ch <= 'f')
    return 10 + (ch - 'a');
  return static_cast<unsigned>(-1);
}

static inline void PercentDecode(const char* input,
                                 size_t len,
                                 std::string* dest) {
  if (len == 0)
    return;
  dest->reserve(len);
  const char* pointer = input;
  const char* end = input + len;
  size_t remaining = pointer - end - 1;
  while (pointer < end) {
    const char ch = pointer[0];
    remaining = (end - pointer) + 1;
    if (ch != '%' || remaining < 2 ||
        (ch == '%' &&
         (!ASCII_HEX_DIGIT(pointer[1]) ||
          !ASCII_HEX_DIGIT(pointer[2])))) {
      *dest += ch;
      pointer++;
      continue;
    } else {
      unsigned a = hex2bin(pointer[1]);
      unsigned b = hex2bin(pointer[2]);
      char c = static_cast<char>(a * 16 + b);
      *dest += c;
      pointer += 3;
    }
  }
}

#define SPECIALS(XX)                                                          \
  XX("ftp:", 21)                                                              \
  XX("file:", -1)                                                             \
  XX("gopher:", 70)                                                           \
  XX("http:", 80)                                                             \
  XX("https:", 443)                                                           \
  XX("ws:", 80)                                                               \
  XX("wss:", 443)

#define PARSESTATES(XX)                                                       \
  XX(kSchemeStart)                                                            \
  XX(kScheme)                                                                 \
  XX(kNoScheme)                                                               \
  XX(kSpecialRelativeOrAuthority)                                             \
  XX(kPathOrAuthority)                                                        \
  XX(kRelative)                                                               \
  XX(kRelativeSlash)                                                          \
  XX(kSpecialAuthoritySlashes)                                                \
  XX(kSpecialAuthorityIgnoreSlashes)                                          \
  XX(kAuthority)                                                              \
  XX(kHost)                                                                   \
  XX(kHostname)                                                               \
  XX(kPort)                                                                   \
  XX(kFile)                                                                   \
  XX(kFileSlash)                                                              \
  XX(kFileHost)                                                               \
  XX(kPathStart)                                                              \
  XX(kPath)                                                                   \
  XX(kCannotBeBase)                                                           \
  XX(kQuery)                                                                  \
  XX(kFragment)

#define FLAGS(XX)                                                             \
  XX(URL_FLAGS_NONE, 0)                                                       \
  XX(URL_FLAGS_FAILED, 0x01)                                                  \
  XX(URL_FLAGS_CANNOT_BE_BASE, 0x02)                                          \
  XX(URL_FLAGS_INVALID_PARSE_STATE, 0x04)                                     \
  XX(URL_FLAGS_TERMINATED, 0x08)                                              \
  XX(URL_FLAGS_SPECIAL, 0x10)                                                 \
  XX(URL_FLAGS_HAS_SCHEME, 0x20)                                              \
  XX(URL_FLAGS_HAS_USERNAME, 0x40)                                            \
  XX(URL_FLAGS_HAS_PASSWORD, 0x80)                                            \
  XX(URL_FLAGS_HAS_HOST, 0x100)                                               \
  XX(URL_FLAGS_HAS_PATH, 0x200)                                               \
  XX(URL_FLAGS_HAS_QUERY, 0x400)                                              \
  XX(URL_FLAGS_HAS_FRAGMENT, 0x800)

#define ARGS(XX)                                                              \
  XX(ARG_FLAGS)                                                               \
  XX(ARG_PROTOCOL)                                                            \
  XX(ARG_USERNAME)                                                            \
  XX(ARG_PASSWORD)                                                            \
  XX(ARG_HOST)                                                                \
  XX(ARG_PORT)                                                                \
  XX(ARG_PATH)                                                                \
  XX(ARG_QUERY)                                                               \
  XX(ARG_FRAGMENT)

#define ERR_ARGS(XX)                                                          \
  XX(ERR_ARG_FLAGS)                                                           \
  XX(ERR_ARG_INPUT)                                                           \

static const char kEOL = -1;

enum url_parse_state {
  kUnknownState = -1,
#define XX(name) name,
  PARSESTATES(XX)
#undef XX
};

enum url_flags {
#define XX(name, val) name = val,
  FLAGS(XX)
#undef XX
};

enum url_cb_args {
#define XX(name) name,
  ARGS(XX)
#undef XX
};

enum url_error_cb_args {
#define XX(name) name,
  ERR_ARGS(XX)
#undef XX
} url_error_cb_args;

static inline bool IsSpecial(std::string scheme) {
#define XX(name, _) if (scheme == name) return true;
  SPECIALS(XX);
#undef XX
  return false;
}

static inline int NormalizePort(std::string scheme, int p) {
#define XX(name, port) if (scheme == name && p == port) return -1;
  SPECIALS(XX);
#undef XX
  return p;
}

struct url_data {
  int32_t flags = URL_FLAGS_NONE;
  int port = -1;
  std::string scheme;
  std::string username;
  std::string password;
  std::string host;
  std::string query;
  std::string fragment;
  std::vector<std::string> path;
};

union url_host_value {
  std::string domain;
  uint32_t ipv4;
  uint16_t ipv6[8];
  ~url_host_value() {}
};

enum url_host_type {
  HOST_TYPE_FAILED = -1,
  HOST_TYPE_DOMAIN = 0,
  HOST_TYPE_IPV4 = 1,
  HOST_TYPE_IPV6 = 2
};

struct url_host {
  url_host_value value;
  enum url_host_type type;
};

class URL {
 public:
  static void Parse(const char* input,
                    const size_t len,
                    enum url_parse_state state_override,
                    struct url_data* url,
                    const struct url_data* base,
                    bool has_base);

  URL(const char* input, const size_t len) {
    Parse(input, len, kUnknownState, &context_, nullptr, false);
  }

  URL(const char* input, const size_t len, const URL* base) {
    if (base != nullptr)
      Parse(input, len, kUnknownState, &context_, &(base->context_), true);
    else
      Parse(input, len, kUnknownState, &context_, nullptr, false);
  }

  URL(const char* input, const size_t len,
      const char* base, const size_t baselen) {
    if (base != nullptr && baselen > 0) {
      URL _base(base, baselen);
      Parse(input, len, kUnknownState, &context_, &(_base.context_), true);
    } else {
      Parse(input, len, kUnknownState, &context_, nullptr, false);
    }
  }

  explicit URL(std::string input) :
      URL(input.c_str(), input.length()) {}

  URL(std::string input, const URL* base) :
      URL(input.c_str(), input.length(), base) {}

  URL(std::string input, std::string base) :
      URL(input.c_str(), input.length(), base.c_str(), base.length()) {}

  int32_t flags() {
    return context_.flags;
  }

  int port() {
    return context_.port;
  }

  const std::string& protocol() const {
    return context_.scheme;
  }

  const std::string& username() const {
    return context_.username;
  }

  const std::string& password() const {
    return context_.password;
  }

  const std::string& host() const {
    return context_.host;
  }

  const std::string& query() const {
    return context_.query;
  }

  const std::string& fragment() const {
    return context_.fragment;
  }

  std::string path() {
    std::string ret;
    for (auto i = context_.path.begin(); i != context_.path.end(); i++) {
      ret += '/';
      ret += *i;
    }
    return ret;
  }

 private:
  struct url_data context_;
};

}  // namespace url

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_URL_H_
