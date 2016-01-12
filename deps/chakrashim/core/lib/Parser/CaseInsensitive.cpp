//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
//
// Map Unicode characters to their equivalence classes induced by the modified ToUpper map.
// i.e.: c1 and c2 are in the same class if ToUpper(c1) == ToUpper(c2).
//
// The ToUpper map takes any character to its Unicode upper case equivalent, with the modification that
// a non-7-bit-ASCII character cannot be mapped to 7-bit-ASCII characters.
//

#include "ParserPch.h"

namespace UnifiedRegex
{
    namespace CaseInsensitive
    {
        struct Transform
        {
            // This skipCount is to help define the range. Ex, given range [0 - 20]
            // If skip count is 1, then all items between 0 and 20 are in the range.
            // If skip count is 2, then every even item is in the range, so 0, 2, 4, 6, 8, etc.
            byte skipCountOfRange;

            MappingSource source;

            // Range of chars this transform applies to
            Chars<codepoint_t>::UChar lo;
            Chars<codepoint_t>::UChar hi;

            // Offsets to add to original character to get each equivalent character
            int delta0;
            int delta1;
            int delta2;
            int delta3;

            template <typename Char>
            inline void Apply(uint c, Char outEquiv[EquivClassSize]) const
            {
                Assert(c >= lo && c <= hi);

                outEquiv[0] = Chars<Char>::UTC((lo + 1) % skipCountOfRange == c % skipCountOfRange ? (int)c + delta0 : c);

                CompileAssert(CaseInsensitive::EquivClassSize == 4);
                if (lo  % skipCountOfRange == c % skipCountOfRange)
                {
                    outEquiv[1] = Chars<Char>::ITC((int)c + delta1);
                    outEquiv[2] = Chars<Char>::ITC((int)c + delta2);
                    outEquiv[3] = Chars<Char>::ITC((int)c + delta3);
                }
                else
                {
                    outEquiv[1] = outEquiv[2] = outEquiv[3] = Chars<Char>::UTC(c);
                }
            }
        };

/*
We first construct a total map from character codes to equivalence lists such that:
 - if ToUpper(c1) == ToUpper(c2) then c1 has c2 in its equivalence list
 - if c1 and c2 appear in the same equivalence list then c1 and c2 have equal equivalence lists

We then compress the above map such that:
 - characters with singleton equivalence classes are elided
 - consecutive characters with consecutive equivalence lists are represented as a range and delta
 - the result is in strictly increasing range order

Using gawk the above is:
  gawk -f equiv.gawk http://www.unicode.org/Public/UNIDATA/UnicodeData.txt | gawk -f table.gawk

Where equiv.gawk is:
----------------------------------------------------------------------
BEGIN {
  FS = ";";
  previncode = -1;
}
length($1) == 4 {
  incode = strtonum("0x" $1);
  for (i = previncode + 1; i < incode; i++)
    map[i] = i;
  if ($3 == "Ll" && $15 != "")
  {
    map[incode] = strtonum("0x" $15);
    # non-7-bit-ASCII cannot map to 7-bit-ASCII
    if (incode > 127 && map[incode] <= 127)
      map[incode] = incode;
  }
  else
    map[incode] = incode;
  previncode = incode;
}
END {
  for (i = previncode + 1; i <= 0xffff; i++)
    map[i] = i;

  for (i = 0x0000; i <= 0xffff; i++)
    ninv[i] = 0;

  for (i = 0x0000; i <= 0xffff; i++)
  {
    if (map[i] != i)
      ninv[map[i]]++;
  }

  maxninv = 0;
  for (i = 0x0000; i <= 0xffff; i++)
  {
    if (ninv[i] > maxninv)
      maxninv = ninv[i];
  }
  if (maxninv > 2)
    print "ERROR";

  for (i = 0x0000; i <= 0xffff; i++)
    inv[i] = "";

  for (i = 0x0000; i <= 0xffff; i++)
  {
    if (map[i] != i)
      inv[map[i]] = sprintf("%s;0x%04x", inv[map[i]], i);
  }

  for (i = 0x0000; i <= 0xffff; i++)
  {
    if (map[i] != i)
    {
      equiv[i] = sprintf("0x%04x%s", map[i], inv[map[i]]);
      nequiv[i] = 1 + ninv[map[i]];
    }
    else if (inv[i] != "")
    {
      equiv[i] = sprintf("0x%04x%s", i, inv[i]);
      nequiv[i] = 1 + ninv[i];
    }
    else
    {
      equiv[i] = sprintf("0x%04x", i);
      nequiv[i] = 1;
    }
  }

  nentries = 0
  for (i = 0x0000; i <= 0xffff; i++)
  {
    if (nequiv[i] > 1)
    {
      printf("0x%04x;%s\n", i, equiv[i]);
      nentries++;
    }
  }
  #printf("nentries = %d\n", nentries);
}
----------------------------------------------------------------------

And table.gawk is:
----------------------------------------------------------------------
BEGIN {
  FS = ";";
  lastCode = -1;
  currStart = -1;
  for (i = 0; i < 3; i++)
    currDeltas[i] = "";
}
{
  if (NF > 4)
    print "ERROR"

  incode = strtonum($1);
  for (i = 0; i < NF - 1; i++)
    equivs[i] = strtonum($(i+2));
  for (i = NF - 1; i < 3; i++)
    equivs[i] = equivs[i - 1];

  #printf("0x%04x, 0x%04x, 0x%04x, 0x%04x\n", incode, equivs[0], equivs[1], equivs[2]);

  for (i = 0; i < 3; i++)
    deltas[i] = equivs[i] - incode;

  if (currStart < 0)
  {
    # start a new range
    currStart = incode;
    for (i = 0; i < 3; i++)
      currDeltas[i] = deltas[i]
  }
  else if (incode == lastCode + 1 && deltas[0] == currDeltas[0] && deltas[1] == currDeltas[1] && deltas[2] == currDeltas[2])
  {
    # keep accumulating range
  }
  else
  {
    # dump current range and start a new one
    printf("            0x%04x, 0x%04x, %d, %d, %d,\n", currStart, lastCode, currDeltas[0], currDeltas[1], currDeltas[2]);
    currStart = incode;
    for (i = 0; i < 3; i++)
      currDeltas[i] = deltas[i]
  }

  lastCode = incode;
}
END {
  printf("            0x%04x, 0x%04x, %d, %d, %d,\n", currStart, lastCode, currDeltas[0], currDeltas[1], currDeltas[2]);
}
----------------------------------------------------------------------
*/

        // For case-folding entries, version 8.0.0 of CaseFolding.txt located at [1] was used.
        // [1] ftp://ftp.unicode.org/Public/UNIDATA/CaseFolding.txt
        static const Transform transforms[] =
        {
            1, MappingSource::UnicodeData, 0x0041, 0x004a, 0, 32, 32, 32,
            1, MappingSource::CaseFolding, 0x004b, 0x004b, 0, 32, 8415, 8415,
            1, MappingSource::UnicodeData, 0x004b, 0x0052, 0, 32, 32, 32,
            1, MappingSource::CaseFolding, 0x0053, 0x0053, 0, 32, 300, 300,
            1, MappingSource::UnicodeData, 0x0053, 0x005a, 0, 32, 32, 32,
            1, MappingSource::UnicodeData, 0x0061, 0x006a, -32, 0, 0, 0,
            1, MappingSource::CaseFolding, 0x006b, 0x006b, -32, 0, 8383, 8383,
            1, MappingSource::UnicodeData, 0x006b, 0x0072, -32, 0, 0, 0,
            1, MappingSource::CaseFolding, 0x0073, 0x0073, -32, 0, 268, 268,
            1, MappingSource::UnicodeData, 0x0073, 0x007a, -32, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x00b5, 0x00b5, 743, 0, 775, 775,
            1, MappingSource::UnicodeData, 0x00c0, 0x00c4, 0, 32, 32, 32,
            1, MappingSource::CaseFolding, 0x00c5, 0x00c5, 0, 32, 8294, 8294,
            1, MappingSource::UnicodeData, 0x00c5, 0x00d6, 0, 32, 32, 32,
            1, MappingSource::UnicodeData, 0x00d8, 0x00de, 0, 32, 32, 32,
            1, MappingSource::UnicodeData, 0x00DF, 0x00DF, 0, 7615, 7615, 7615,
            1, MappingSource::UnicodeData, 0x00e0, 0x00e4, -32, 0, 0, 0,
            1, MappingSource::CaseFolding, 0x00e5, 0x00e5, -32, 0, 8262, 8262,
            1, MappingSource::UnicodeData, 0x00e5, 0x00f6, -32, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x00f8, 0x00fe, -32, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x00ff, 0x00ff, 121, 0, 0, 0,
            2, MappingSource::UnicodeData, 0x0100, 0x012f, -1, 1, 1, 1,
            2, MappingSource::UnicodeData, 0x0132, 0x0137, -1, 1, 1, 1,
            2, MappingSource::UnicodeData, 0x0139, 0x0148, -1, 1, 1, 1,
            2, MappingSource::UnicodeData, 0x014a, 0x0177, -1, 1, 1, 1,
            1, MappingSource::UnicodeData, 0x0178, 0x0178, 0, -121, -121, -121,
            2, MappingSource::UnicodeData, 0x0179, 0x017e, -1, 1, 1, 1,
            1, MappingSource::CaseFolding, 0x017f, 0x017f, -300, -268, 0, 0,
            1, MappingSource::UnicodeData, 0x0180, 0x0180, 195, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x0181, 0x0181, 0, 210, 210, 210,
            2, MappingSource::UnicodeData, 0x0182, 0x0185, -1, 1, 1, 1,
            1, MappingSource::UnicodeData, 0x0186, 0x0186, 0, 206, 206, 206,
            1, MappingSource::UnicodeData, 0x0187, 0x0187, 0, 1, 1, 1,
            1, MappingSource::UnicodeData, 0x0188, 0x0188, -1, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x0189, 0x018a, 0, 205, 205, 205,
            1, MappingSource::UnicodeData, 0x018b, 0x018b, 0, 1, 1, 1,
            1, MappingSource::UnicodeData, 0x018c, 0x018c, -1, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x018e, 0x018e, 0, 79, 79, 79,
            1, MappingSource::UnicodeData, 0x018f, 0x018f, 0, 202, 202, 202,
            1, MappingSource::UnicodeData, 0x0190, 0x0190, 0, 203, 203, 203,
            1, MappingSource::UnicodeData, 0x0191, 0x0191, 0, 1, 1, 1,
            1, MappingSource::UnicodeData, 0x0192, 0x0192, -1, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x0193, 0x0193, 0, 205, 205, 205,
            1, MappingSource::UnicodeData, 0x0194, 0x0194, 0, 207, 207, 207,
            1, MappingSource::UnicodeData, 0x0195, 0x0195, 97, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x0196, 0x0196, 0, 211, 211, 211,
            1, MappingSource::UnicodeData, 0x0197, 0x0197, 0, 209, 209, 209,
            1, MappingSource::UnicodeData, 0x0198, 0x0198, 0, 1, 1, 1,
            1, MappingSource::UnicodeData, 0x0199, 0x0199, -1, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x019a, 0x019a, 163, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x019c, 0x019c, 0, 211, 211, 211,
            1, MappingSource::UnicodeData, 0x019d, 0x019d, 0, 213, 213, 213,
            1, MappingSource::UnicodeData, 0x019e, 0x019e, 130, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x019f, 0x019f, 0, 214, 214, 214,
            2, MappingSource::UnicodeData, 0x01a0, 0x01a5, -1, 1, 1, 1,
            1, MappingSource::UnicodeData, 0x01a6, 0x01a6, 0, 218, 218, 218,
            1, MappingSource::UnicodeData, 0x01a7, 0x01a7, 0, 1, 1, 1,
            1, MappingSource::UnicodeData, 0x01a8, 0x01a8, -1, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x01a9, 0x01a9, 0, 218, 218, 218,
            1, MappingSource::UnicodeData, 0x01ac, 0x01ac, 0, 1, 1, 1,
            1, MappingSource::UnicodeData, 0x01ad, 0x01ad, -1, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x01ae, 0x01ae, 0, 218, 218, 218,
            1, MappingSource::UnicodeData, 0x01af, 0x01af, 0, 1, 1, 1,
            1, MappingSource::UnicodeData, 0x01b0, 0x01b0, -1, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x01b1, 0x01b2, 0, 217, 217, 217,
            1, MappingSource::UnicodeData, 0x01b3, 0x01b3, 0, 1, 1, 1,
            1, MappingSource::UnicodeData, 0x01b4, 0x01b4, -1, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x01b5, 0x01b5, 0, 1, 1, 1,
            1, MappingSource::UnicodeData, 0x01b6, 0x01b6, -1, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x01b7, 0x01b7, 0, 219, 219, 219,
            2, MappingSource::UnicodeData, 0x01b8, 0x01bd, -1, 1, 1, 1,
            1, MappingSource::UnicodeData, 0x01bf, 0x01bf, 56, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x01c4, 0x01c4, 2, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x01c5, 0x01c5, 1, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x01c6, 0x01c6, 0, -2, -1, -1,
            1, MappingSource::UnicodeData, 0x01c7, 0x01c7, 2, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x01c8, 0x01c8, 1, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x01c9, 0x01c9, 0, -2, -1, -1,
            1, MappingSource::UnicodeData, 0x01CA, 0x01CA, 2, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x01CB, 0x01CB, 1, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x01CC, 0x01CC, 0, -2, -1, -1,
            2, MappingSource::UnicodeData, 0x01cd, 0x01dc, -1, 1, 1, 1,
            1, MappingSource::UnicodeData, 0x01dd, 0x01dd, -79, 0, 0, 0,
            2, MappingSource::UnicodeData, 0x01de, 0x01f5, -1, 1, 1, 1,
            1, MappingSource::UnicodeData, 0x01F1, 0x01F1, 2, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x01F2, 0x01F2, 1, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x01F3, 0x01F3, 0, -2, -1, -1,
            2, MappingSource::UnicodeData, 0x01f4, 0x01f5, -1, 1, 1, 1,
            1, MappingSource::UnicodeData, 0x01f6, 0x01f6, 0, -97, -97, -97,
            1, MappingSource::UnicodeData, 0x01f7, 0x01f7, 0, -56, -56, -56,
            2, MappingSource::UnicodeData, 0x01f8, 0x021f, -1, 1, 1, 1,
            1, MappingSource::UnicodeData, 0x0220, 0x0220, 0, -130, -130, -130,
            2, MappingSource::UnicodeData, 0x0222, 0x0233, -1, 1, 1, 1,
            1, MappingSource::UnicodeData, 0x023a, 0x023a, 0, 10795, 10795, 10795,
            1, MappingSource::UnicodeData, 0x023b, 0x023b, 0, 1, 1, 1,
            1, MappingSource::UnicodeData, 0x023c, 0x023c, -1, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x023d, 0x023d, 0, -163, -163, -163,
            1, MappingSource::UnicodeData, 0x023e, 0x023e, 0, 10792, 10792, 10792,
            1, MappingSource::UnicodeData, 0x023f, 0x0240, 10815, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x0241, 0x0241, 0, 1, 1, 1,
            1, MappingSource::UnicodeData, 0x0242, 0x0242, -1, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x0243, 0x0243, 0, -195, -195, -195,
            1, MappingSource::UnicodeData, 0x0244, 0x0244, 0, 69, 69, 69,
            1, MappingSource::UnicodeData, 0x0245, 0x0245, 0, 71, 71, 71,
            2, MappingSource::UnicodeData, 0x0246, 0x024f, -1, 1, 1, 1,
            1, MappingSource::UnicodeData, 0x0250, 0x0250, 10783, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x0251, 0x0251, 10780, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x0252, 0x0252, 10782, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x0253, 0x0253, -210, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x0254, 0x0254, -206, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x0256, 0x0257, -205, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x0259, 0x0259, -202, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x025b, 0x025b, -203, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x0260, 0x0260, -205, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x0263, 0x0263, -207, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x0265, 0x0265, 42280, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x0266, 0x0266, 42308, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x0268, 0x0268, -209, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x0269, 0x0269, -211, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x026b, 0x026b, 10743, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x026f, 0x026f, -211, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x0271, 0x0271, 10749, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x0272, 0x0272, -213, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x0275, 0x0275, -214, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x027d, 0x027d, 10727, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x0280, 0x0280, -218, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x0283, 0x0283, -218, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x0288, 0x0288, -218, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x0289, 0x0289, -69, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x028a, 0x028b, -217, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x028c, 0x028c, -71, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x0292, 0x0292, -219, 0, 0, 0,
            1, MappingSource::CaseFolding, 0x0345, 0x0345, 0, 84, 116, 7289,
            2, MappingSource::UnicodeData, 0x0370, 0x0373, -1, 1, 1, 1,
            1, MappingSource::UnicodeData, 0x0376, 0x0376, 0, 1, 1, 1,
            1, MappingSource::UnicodeData, 0x0377, 0x0377, -1, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x037b, 0x037d, 130, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x0386, 0x0386, 0, 38, 38, 38,
            1, MappingSource::UnicodeData, 0x0388, 0x038a, 0, 37, 37, 37,
            1, MappingSource::UnicodeData, 0x038c, 0x038c, 0, 64, 64, 64,
            1, MappingSource::UnicodeData, 0x038e, 0x038f, 0, 63, 63, 63,
            1, MappingSource::UnicodeData, 0x0391, 0x0391, 0, 32, 32, 32,
            1, MappingSource::UnicodeData, 0x0392, 0x0392, 0, 32, 62, 62,
            1, MappingSource::UnicodeData, 0x0393, 0x0394, 0, 32, 32, 32,
            1, MappingSource::UnicodeData, 0x0395, 0x0395, 0, 32, 96, 96,
            1, MappingSource::UnicodeData, 0x0396, 0x0397, 0, 32, 32, 32,
            1, MappingSource::CaseFolding, 0x0398, 0x0398, 0, 32, 57, 92,
            1, MappingSource::UnicodeData, 0x0398, 0x0398, 0, 32, 57, 57,
            1, MappingSource::CaseFolding, 0x0399, 0x0399, -84, 0, 32, 7205,
            1, MappingSource::UnicodeData, 0x0399, 0x0399, 0, 32, 7205, 7205,
            1, MappingSource::UnicodeData, 0x039a, 0x039a, 0, 32, 86, 86,
            1, MappingSource::UnicodeData, 0x039b, 0x039b, 0, 32, 32, 32,
            1, MappingSource::UnicodeData, 0x039c, 0x039c, 0, -743, 32, 32,
            1, MappingSource::UnicodeData, 0x039d, 0x039f, 0, 32, 32, 32,
            1, MappingSource::UnicodeData, 0x03a0, 0x03a0, 0, 32, 54, 54,
            1, MappingSource::UnicodeData, 0x03a1, 0x03a1, 0, 32, 80, 80,
            1, MappingSource::UnicodeData, 0x03a3, 0x03a3, 0, 31, 32, 32,
            1, MappingSource::UnicodeData, 0x03a4, 0x03a5, 0, 32, 32, 32,
            1, MappingSource::UnicodeData, 0x03a6, 0x03a6, 0, 32, 47, 47,
            1, MappingSource::UnicodeData, 0x03a7, 0x03a8, 0, 32, 32, 32,
            1, MappingSource::CaseFolding, 0x03a9, 0x03a9, 0, 32, 7549, 7549,
            1, MappingSource::UnicodeData, 0x03a9, 0x03ab, 0, 32, 32, 32,
            1, MappingSource::UnicodeData, 0x03ac, 0x03ac, -38, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x03ad, 0x03af, -37, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x03b1, 0x03b1, -32, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x03b2, 0x03b2, -32, 0, 30, 30,
            1, MappingSource::UnicodeData, 0x03b3, 0x03b4, -32, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x03b5, 0x03b5, -32, 0, 64, 64,
            1, MappingSource::UnicodeData, 0x03b6, 0x03b7, -32, 0, 0, 0,
            1, MappingSource::CaseFolding, 0x03b8, 0x03b8, -32, 0, 25, 60,
            1, MappingSource::UnicodeData, 0x03b8, 0x03b8, -32, 0, 25, 25,
            1, MappingSource::CaseFolding, 0x03b9, 0x03b9, -116, -32, 0, 7173,
            1, MappingSource::UnicodeData, 0x03b9, 0x03b9, -32, 0, 7173, 7173,
            1, MappingSource::UnicodeData, 0x03ba, 0x03ba, -32, 0, 54, 54,
            1, MappingSource::UnicodeData, 0x03bb, 0x03bb, -32, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x03bc, 0x03bc, -32, -775, 0, 0,
            1, MappingSource::UnicodeData, 0x03bd, 0x03bf, -32, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x03c0, 0x03c0, -32, 0, 22, 22,
            1, MappingSource::UnicodeData, 0x03c1, 0x03c1, -32, 0, 48, 48,
            1, MappingSource::UnicodeData, 0x03c2, 0x03c2, -31, 0, 1, 1,
            1, MappingSource::UnicodeData, 0x03c3, 0x03c3, -32, -1, 0, 0,
            1, MappingSource::UnicodeData, 0x03c4, 0x03c5, -32, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x03c6, 0x03c6, -32, 0, 15, 15,
            1, MappingSource::UnicodeData, 0x03c7, 0x03c8, -32, 0, 0, 0,
            1, MappingSource::CaseFolding, 0x03c9, 0x03c9, -32, 0, 7517, 7517,
            1, MappingSource::UnicodeData, 0x03c9, 0x03cb, -32, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x03cc, 0x03cc, -64, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x03cd, 0x03ce, -63, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x03cf, 0x03cf, 0, 8, 8, 8,
            1, MappingSource::UnicodeData, 0x03d0, 0x03d0, -62, -30, 0, 0,
            1, MappingSource::CaseFolding, 0x03d1, 0x03d1, -57, -25, 0, 35,
            1, MappingSource::UnicodeData, 0x03d1, 0x03d1, -57, -25, 0, 0,
            1, MappingSource::UnicodeData, 0x03d5, 0x03d5, -47, -15, 0, 0,
            1, MappingSource::UnicodeData, 0x03d6, 0x03d6, -54, -22, 0, 0,
            1, MappingSource::UnicodeData, 0x03d7, 0x03d7, -8, 0, 0, 0,
            2, MappingSource::UnicodeData, 0x03d8, 0x03ef, -1, 1, 1, 1,
            1, MappingSource::UnicodeData, 0x03f0, 0x03f0, -86, -54, 0, 0,
            1, MappingSource::UnicodeData, 0x03f1, 0x03f1, -80, -48, 0, 0,
            1, MappingSource::UnicodeData, 0x03f2, 0x03f2, 7, 0, 0, 0,
            1, MappingSource::CaseFolding, 0x03f4, 0x03f4, -92, -60, -35, 0,
            1, MappingSource::UnicodeData, 0x03f5, 0x03f5, -96, -64, 0, 0,
            1, MappingSource::UnicodeData, 0x03f7, 0x03f7, 0, 1, 1, 1,
            1, MappingSource::UnicodeData, 0x03f8, 0x03f8, -1, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x03f9, 0x03f9, 0, -7, -7, -7,
            1, MappingSource::UnicodeData, 0x03fa, 0x03fa, 0, 1, 1, 1,
            1, MappingSource::UnicodeData, 0x03fb, 0x03fb, -1, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x03fd, 0x03ff, 0, -130, -130, -130,
            1, MappingSource::UnicodeData, 0x0400, 0x040f, 0, 80, 80, 80,
            1, MappingSource::UnicodeData, 0x0410, 0x042f, 0, 32, 32, 32,
            1, MappingSource::UnicodeData, 0x0430, 0x044f, -32, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x0450, 0x045f, -80, 0, 0, 0,
            2, MappingSource::UnicodeData, 0x0460, 0x0481, -1, 1, 1, 1,
            2, MappingSource::UnicodeData, 0x048a, 0x04bf, -1, 1, 1, 1,
            1, MappingSource::UnicodeData, 0x04c0, 0x04c0, 0, 15, 15, 15,
            2, MappingSource::UnicodeData, 0x04c1, 0x04ce, -1, 1, 1, 1,
            1, MappingSource::UnicodeData, 0x04cf, 0x04cf, -15, 0, 0, 0,
            2, MappingSource::UnicodeData, 0x04d0, 0x0527, -1, 1, 1, 1,
            1, MappingSource::UnicodeData, 0x0531, 0x0556, 0, 48, 48, 48,
            1, MappingSource::UnicodeData, 0x0561, 0x0586, -48, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x10a0, 0x10c5, 0, 7264, 7264, 7264,
            1, MappingSource::UnicodeData, 0x10C7, 0x10C7, 0, 7264, 7264, 7264,
            1, MappingSource::UnicodeData, 0x10CD, 0x10CD, 0, 7264, 7264, 7264,
            1, MappingSource::UnicodeData, 0x1d79, 0x1d79, 35332, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x1d7d, 0x1d7d, 3814, 0, 0, 0,
            2, MappingSource::UnicodeData, 0x1e00, 0x1e5f, -1, 1, 1, 1,
            1, MappingSource::UnicodeData, 0x1e60, 0x1e60, 0, 1, 59, 59,
            1, MappingSource::UnicodeData, 0x1e61, 0x1e61, -1, 0, 58, 58,
            2, MappingSource::UnicodeData, 0x1e62, 0x1e95, -1, 1, 1, 1,
            1, MappingSource::UnicodeData, 0x1e9b, 0x1e9b, -59, -58, 0, 0,
            1, MappingSource::UnicodeData, 0x1E9E, 0x1E9E, -7615, 0, 0, 0,
            2, MappingSource::UnicodeData, 0x1ea0, 0x1eff, -1, 1, 1, 1,
            1, MappingSource::UnicodeData, 0x1f00, 0x1f07, 8, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x1f08, 0x1f0f, 0, -8, -8, -8,
            1, MappingSource::UnicodeData, 0x1f10, 0x1f15, 8, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x1f18, 0x1f1d, 0, -8, -8, -8,
            1, MappingSource::UnicodeData, 0x1f20, 0x1f27, 8, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x1f28, 0x1f2f, 0, -8, -8, -8,
            1, MappingSource::UnicodeData, 0x1f30, 0x1f37, 8, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x1f38, 0x1f3f, 0, -8, -8, -8,
            1, MappingSource::UnicodeData, 0x1f40, 0x1f45, 8, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x1f48, 0x1f4d, 0, -8, -8, -8,
            1, MappingSource::UnicodeData, 0x1f51, 0x1f51, 8, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x1f53, 0x1f53, 8, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x1f55, 0x1f55, 8, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x1f57, 0x1f57, 8, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x1f59, 0x1f59, 0, -8, -8, -8,
            1, MappingSource::UnicodeData, 0x1f5b, 0x1f5b, 0, -8, -8, -8,
            1, MappingSource::UnicodeData, 0x1f5d, 0x1f5d, 0, -8, -8, -8,
            1, MappingSource::UnicodeData, 0x1f5f, 0x1f5f, 0, -8, -8, -8,
            1, MappingSource::UnicodeData, 0x1f60, 0x1f67, 8, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x1f68, 0x1f6f, 0, -8, -8, -8,
            1, MappingSource::UnicodeData, 0x1f70, 0x1f71, 74, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x1f72, 0x1f75, 86, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x1f76, 0x1f77, 100, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x1f78, 0x1f79, 128, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x1f7a, 0x1f7b, 112, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x1f7c, 0x1f7d, 126, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x1f80, 0x1f87, 8, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x1f88, 0x1f8f, 0, -8, -8, -8,
            1, MappingSource::UnicodeData, 0x1f90, 0x1f97, 8, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x1f98, 0x1f9f, 0, -8, -8, -8,
            1, MappingSource::UnicodeData, 0x1fa0, 0x1fa7, 8, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x1fa8, 0x1faf, 0, -8, -8, -8,
            1, MappingSource::UnicodeData, 0x1fb0, 0x1fb1, 8, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x1fb3, 0x1fb3, 9, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x1fb8, 0x1fb9, 0, -8, -8, -8,
            1, MappingSource::UnicodeData, 0x1fba, 0x1fbb, 0, -74, -74, -74,
            1, MappingSource::UnicodeData, 0x1fbc, 0x1fbc, 0, -9, -9, -9,
            1, MappingSource::CaseFolding, 0x1fbe, 0x1fbe, -7289, -7205, -7173, 0,
            1, MappingSource::UnicodeData, 0x1fbe, 0x1fbe, -7205, -7173, 0, 0,
            1, MappingSource::UnicodeData, 0x1fc3, 0x1fc3, 9, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x1fc8, 0x1fcb, 0, -86, -86, -86,
            1, MappingSource::UnicodeData, 0x1fcc, 0x1fcc, 0, -9, -9, -9,
            1, MappingSource::UnicodeData, 0x1fd0, 0x1fd1, 8, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x1fd8, 0x1fd9, 0, -8, -8, -8,
            1, MappingSource::UnicodeData, 0x1fda, 0x1fdb, 0, -100, -100, -100,
            1, MappingSource::UnicodeData, 0x1fe0, 0x1fe1, 8, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x1fe5, 0x1fe5, 7, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x1fe8, 0x1fe9, 0, -8, -8, -8,
            1, MappingSource::UnicodeData, 0x1fea, 0x1feb, 0, -112, -112, -112,
            1, MappingSource::UnicodeData, 0x1fec, 0x1fec, 0, -7, -7, -7,
            1, MappingSource::UnicodeData, 0x1ff3, 0x1ff3, 9, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x1ff8, 0x1ff9, 0, -128, -128, -128,
            1, MappingSource::UnicodeData, 0x1ffa, 0x1ffb, 0, -126, -126, -126,
            1, MappingSource::UnicodeData, 0x1ffc, 0x1ffc, 0, -9, -9, -9,
            1, MappingSource::CaseFolding, 0x2126, 0x2126, -7549, -7517, 0, 0,
            1, MappingSource::CaseFolding, 0x212a, 0x212a, -8415, -8383, 0, 0,
            1, MappingSource::CaseFolding, 0x212b, 0x212b, -8294, -8262, 0, 0,
            1, MappingSource::UnicodeData, 0x2132, 0x2132, 0, 28, 28, 28,
            1, MappingSource::UnicodeData, 0x214e, 0x214e, -28, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x2160, 0x216F, 0, 16, 16, 16,
            1, MappingSource::UnicodeData, 0x2170, 0x217F, -16, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x2183, 0x2183, 0, 1, 1, 1,
            1, MappingSource::UnicodeData, 0x2184, 0x2184, -1, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x24B6, 0x24CF, 0, 26, 26, 26,
            1, MappingSource::UnicodeData, 0x24D0, 0x24E9, -26, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x2c00, 0x2c2e, 0, 48, 48, 48,
            1, MappingSource::UnicodeData, 0x2c30, 0x2c5e, -48, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x2c60, 0x2c60, 0, 1, 1, 1,
            1, MappingSource::UnicodeData, 0x2c61, 0x2c61, -1, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x2c62, 0x2c62, 0, -10743, -10743, -10743,
            1, MappingSource::UnicodeData, 0x2c63, 0x2c63, 0, -3814, -3814, -3814,
            1, MappingSource::UnicodeData, 0x2c64, 0x2c64, 0, -10727, -10727, -10727,
            1, MappingSource::UnicodeData, 0x2c65, 0x2c65, -10795, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x2c66, 0x2c66, -10792, 0, 0, 0,
            2, MappingSource::UnicodeData, 0x2c67, 0x2c6c, -1, 1, 1, 1,
            1, MappingSource::UnicodeData, 0x2c6d, 0x2c6d, 0, -10780, -10780, -10780,
            1, MappingSource::UnicodeData, 0x2c6e, 0x2c6e, 0, -10749, -10749, -10749,
            1, MappingSource::UnicodeData, 0x2c6f, 0x2c6f, 0, -10783, -10783, -10783,
            1, MappingSource::UnicodeData, 0x2c70, 0x2c70, 0, -10782, -10782, -10782,
            1, MappingSource::UnicodeData, 0x2c72, 0x2c72, 0, 1, 1, 1,
            1, MappingSource::UnicodeData, 0x2c73, 0x2c73, -1, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x2c75, 0x2c75, 0, 1, 1, 1,
            1, MappingSource::UnicodeData, 0x2c76, 0x2c76, -1, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x2c7e, 0x2c7f, 0, -10815, -10815, -10815,
            2, MappingSource::UnicodeData, 0x2c80, 0x2ce3, -1, 1, 1, 1,
            2, MappingSource::UnicodeData, 0x2ceb, 0x2cee, -1, 1, 1, 1,
            2, MappingSource::UnicodeData, 0x2CF2, 0x2CF3, -1, 1, 1, 1,
            1, MappingSource::UnicodeData, 0x2d00, 0x2d25, -7264, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x2D27, 0x2D27, -7264, 0, 0, 0,
            1, MappingSource::UnicodeData, 0x2D2D, 0x2D2D, -7264, 0, 0, 0,
            2, MappingSource::UnicodeData, 0xa640, 0xa66d, -1, 1, 1, 1,
            2, MappingSource::UnicodeData, 0xa680, 0xa697, -1, 1, 1, 1,
            2, MappingSource::UnicodeData, 0xa722, 0xa72f, -1, 1, 1, 1,
            2, MappingSource::UnicodeData, 0xa732, 0xa76f, -1, 1, 1, 1,
            2, MappingSource::UnicodeData, 0xa779, 0xa77c, -1, 1, 1, 1,
            1, MappingSource::UnicodeData, 0xa77d, 0xa77d, 0, -35332, -35332, -35332,
            2, MappingSource::UnicodeData, 0xa77e, 0xa787, -1, 1, 1, 1,
            1, MappingSource::UnicodeData, 0xa78b, 0xa78b, 0, 1, 1, 1,
            1, MappingSource::UnicodeData, 0xa78c, 0xa78c, -1, 0, 0, 0,
            1, MappingSource::UnicodeData, 0xa78d, 0xa78d, 0, -42280, -42280, -42280,
            1, MappingSource::UnicodeData, 0xa790, 0xa790, 0, 1, 1, 1,
            1, MappingSource::UnicodeData, 0xa791, 0xa791, -1, 0, 0, 0,
            2, MappingSource::UnicodeData, 0xa792, 0xa793, -1, 1, 1, 1,
            2, MappingSource::UnicodeData, 0xa7a0, 0xa7a9, -1, 1, 1, 1,
            1, MappingSource::UnicodeData, 0xA7AA, 0XA7AA, 0, -42308, -42308, -42308,
            1, MappingSource::UnicodeData, 0xff21, 0xff3a, 0, 32, 32, 32,
            1, MappingSource::UnicodeData, 0xff41, 0xff5a, -32, 0, 0, 0,
            1, MappingSource::CaseFolding, 0x10400, 0x10427, 0, 40, 40, 40,
            1, MappingSource::CaseFolding, 0x10428, 0x1044f, -40, 0, 0, 0,
            1, MappingSource::CaseFolding, 0x10c80, 0x10cb2, 0, 64, 64, 64,
            1, MappingSource::CaseFolding, 0x10cc0, 0x10cf2, -64, 0, 0, 0,
            1, MappingSource::CaseFolding, 0x118a0, 0x118bf, 0, 32, 32, 32,
            1, MappingSource::CaseFolding, 0x118c0, 0x118df, -32, 0, 0, 0,
        };

        static const int numTransforms = sizeof(transforms) / sizeof(Transform);
        static const Transform lastTransform = transforms[numTransforms - 1];

        template <typename Char, typename Fn>
        bool RangeToEquivClass(uint& tblidx, uint l, uint h, uint& acth, Char equivl[EquivClassSize], Fn acceptSource)
        {
            Assert(l <= h);

            if (lastTransform.hi >= l)
            {
                // Skip transforms which come completely before l
                while (tblidx < numTransforms && (transforms[tblidx].hi < l || !acceptSource(transforms[tblidx].source)))
                {
                    tblidx++;
                }

                if (tblidx < numTransforms)
                {
                    // Does current transform intersect the desired range?
                    uint interl = max(l, static_cast<uint>(transforms[tblidx].lo));
                    uint interh = min(h, static_cast<uint>(transforms[tblidx].skipCountOfRange == 1 ? transforms[tblidx].hi : interl));
                    if (interl <= interh)
                    {
                        if (l < interl)
                        {
                            // Part of input range comes before next table range, so that sub-range has trivial equivalence class
                            acth = interl - 1;
                            for (int i = 0; i < EquivClassSize; i++)
                                equivl[i] = Chars<Char>::UTC(l);
                            return false; // trivial
                        }
                        else
                        {
                            // Input range begins at a table range, so map the character range
                            acth = interh;
                            transforms[tblidx].Apply(interl, equivl);
                            return true; // non-trivial
                        }
                    }
                    // else fall-through: No intersection, so nothing in this range has non-trivial equivalence class
                }
            }
            // else fall-through: No more transforms, so nothing in this range has a non-trivial equivalence class

            acth = h;
            for (int i = 0; i < EquivClassSize; i++)
            {
                equivl[i] = Chars<Char>::UTC(l);
            }
            return false; // trivial
        }

        bool RangeToEquivClass(uint & tblidx, uint l, uint h, uint & acth, __out_ecount(EquivClassSize) wchar_t equivl[EquivClassSize])
        {
            return RangeToEquivClass(tblidx, l, h, acth, equivl, [](MappingSource source) {
                return source == MappingSource::UnicodeData;
            });
        }

        bool RangeToEquivClass(uint & tblidx, uint l, uint h, uint & acth, __out_ecount(EquivClassSize) codepoint_t equivl[EquivClassSize])
        {
            return RangeToEquivClass(tblidx, l, h, acth, equivl, [](MappingSource source) {
                return source == MappingSource::CaseFolding || source == MappingSource::UnicodeData;
            });
        }

        bool RangeToEquivClassOnlyInSource(MappingSource mappingSource, uint& tblidx, uint l, uint h, uint& acth, __out_ecount(EquivClassSize) wchar_t equivl[EquivClassSize])
        {
            return RangeToEquivClass(tblidx, l, h, acth, equivl, [&](MappingSource actualSource) {
                return mappingSource == actualSource;
            });
        }
    }
}
