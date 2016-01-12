//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "ParserPch.h"
#include "..\Runtime\Base\WindowsGlobalizationAdapter.h"

using namespace Windows::Data::Text;

static const CharTypeFlags charFlags[128] =
{
    UnknownChar,                 /* 0x00   */
    UnknownChar,                 /* 0x01   */
    UnknownChar,                 /* 0x02   */
    UnknownChar,                 /* 0x03   */
    UnknownChar,                 /* 0x04   */
    UnknownChar,                 /* 0x05   */
    UnknownChar,                 /* 0x06   */
    UnknownChar,                 /* 0x07   */
    UnknownChar,                 /* 0x08   */
    SpaceChar,                   /* 0x09   */
    LineCharGroup,               /* 0x0A   */
    SpaceChar,                   /* 0x0B   */
    SpaceChar,                   /* 0x0C   */
    LineCharGroup,               /* 0x0D   */
    UnknownChar,                 /* 0x0E   */
    UnknownChar,                 /* 0x0F   */
    UnknownChar,                 /* 0x10   */
    UnknownChar,                 /* 0x11   */
    UnknownChar,                 /* 0x12   */
    UnknownChar,                 /* 0x13   */
    UnknownChar,                 /* 0x14   */
    UnknownChar,                 /* 0x15   */
    UnknownChar,                 /* 0x16   */
    UnknownChar,                 /* 0x17   */
    UnknownChar,                 /* 0x18   */
    UnknownChar,                 /* 0x19   */
    UnknownChar,                 /* 0x1A   */
    UnknownChar,                 /* 0x1B   */
    UnknownChar,                 /* 0x1C   */
    UnknownChar,                 /* 0x1D   */
    UnknownChar,                 /* 0x1E   */
    UnknownChar,                 /* 0x1F   */
    SpaceChar,                   /* 0x20   */
    UnknownChar,                 /* 0x21 ! */
    UnknownChar,                 /* 0x22   */
    UnknownChar,                 /* 0x23 # */
    LetterCharGroup,             /* 0x24 $ */
    UnknownChar,                 /* 0x25 % */
    UnknownChar,                 /* 0x26 & */
    UnknownChar,                 /* 0x27   */
    UnknownChar,                 /* 0x28   */
    UnknownChar,                 /* 0x29   */
    UnknownChar,                 /* 0x2A   */
    UnknownChar,                 /* 0x2B   */
    UnknownChar,                 /* 0x2C   */
    UnknownChar,                 /* 0x2D   */
    UnknownChar,                 /* 0x2E   */
    UnknownChar,                 /* 0x2F   */
    DecimalCharGroup,            /* 0x30 0 */
    DecimalCharGroup,            /* 0x31 1 */
    DecimalCharGroup,            /* 0x32 2 */
    DecimalCharGroup,            /* 0x33 3 */
    DecimalCharGroup,            /* 0x34 4 */
    DecimalCharGroup,            /* 0x35 5 */
    DecimalCharGroup,            /* 0x36 6 */
    DecimalCharGroup,            /* 0x37 7 */
    DecimalCharGroup,            /* 0x38 8 */
    DecimalCharGroup,            /* 0x39 9 */
    UnknownChar,                 /* 0x3A   */
    UnknownChar,                 /* 0x3B   */
    UnknownChar,                 /* 0x3C < */
    UnknownChar,                 /* 0x3D = */
    UnknownChar,                 /* 0x3E > */
    UnknownChar,                 /* 0x3F   */
    UnknownChar,                 /* 0x40 @ */
    HexCharGroup,                /* 0x41 A */
    HexCharGroup,                /* 0x42 B */
    HexCharGroup,                /* 0x43 C */
    HexCharGroup,                /* 0x44 D */
    HexCharGroup,                /* 0x45 E */
    HexCharGroup,                /* 0x46 F */
    LetterCharGroup,             /* 0x47 G */
    LetterCharGroup,             /* 0x48 H */
    LetterCharGroup,             /* 0x49 I */
    LetterCharGroup,             /* 0x4A J */
    LetterCharGroup,             /* 0x4B K */
    LetterCharGroup,             /* 0x4C L */
    LetterCharGroup,             /* 0x4D M */
    LetterCharGroup,             /* 0x4E N */
    LetterCharGroup,             /* 0x4F O */
    LetterCharGroup,             /* 0x50 P */
    LetterCharGroup,             /* 0x51 Q */
    LetterCharGroup,             /* 0x52 R */
    LetterCharGroup,             /* 0x53 S */
    LetterCharGroup,             /* 0x54 T */
    LetterCharGroup,             /* 0x55 U */
    LetterCharGroup,             /* 0x56 V */
    LetterCharGroup,             /* 0x57 W */
    LetterCharGroup,             /* 0x58 X */
    LetterCharGroup,             /* 0x59 Y */
    LetterCharGroup,             /* 0x5A Z */
    UnknownChar,                 /* 0x5B   */
    UnknownChar,                 /* 0x5C   */
    UnknownChar,                 /* 0x5D   */
    UnknownChar,                 /* 0x5E   */
    LetterCharGroup,             /* 0x5F _ */
    UnknownChar,                 /* 0x60   */
    HexCharGroup,                /* 0x61 a */
    HexCharGroup,                /* 0x62 b */
    HexCharGroup,                /* 0x63 c */
    HexCharGroup,                /* 0x64 d */
    HexCharGroup,                /* 0x65 e */
    HexCharGroup,                /* 0x66 f */
    LetterCharGroup,             /* 0x67 g */
    LetterCharGroup,             /* 0x68 h */
    LetterCharGroup,             /* 0x69 i */
    LetterCharGroup,             /* 0x6A j */
    LetterCharGroup,             /* 0x6B k */
    LetterCharGroup,             /* 0x6C l */
    LetterCharGroup,             /* 0x6D m */
    LetterCharGroup,             /* 0x6E n */
    LetterCharGroup,             /* 0x6F o */
    LetterCharGroup,             /* 0x70 p */
    LetterCharGroup,             /* 0x71 q */
    LetterCharGroup,             /* 0x72 r */
    LetterCharGroup,             /* 0x73 s */
    LetterCharGroup,             /* 0x74 t */
    LetterCharGroup,             /* 0x75 u */
    LetterCharGroup,             /* 0x76 v */
    LetterCharGroup,             /* 0x77 w */
    LetterCharGroup,             /* 0x78 x */
    LetterCharGroup,             /* 0x79 y */
    LetterCharGroup,             /* 0x7A z */
    UnknownChar,                 /* 0x7B   */
    UnknownChar,                 /* 0x7C   */
    UnknownChar,                 /* 0x7D   */
    UnknownChar,                 /* 0x7E   */
    UnknownChar                  /* 0x7F   */
};

    /*****************************************************************************
*
*  The _C_xxx enum and charTypes[] table are used to map a character to
*  simple classification values and flags.
*/

static const CharTypes charTypes[128] =
{
    _C_NUL, _C_ERR, _C_ERR, _C_ERR, _C_ERR, _C_ERR, _C_ERR, _C_ERR,     /* 00-07 */
    _C_ERR, _C_WSP, _C_NWL, _C_WSP, _C_WSP, _C_NWL, _C_ERR, _C_ERR,     /* 08-0F */

    _C_ERR, _C_ERR, _C_ERR, _C_ERR, _C_ERR, _C_ERR, _C_ERR, _C_ERR,     /* 10-17 */
    _C_ERR, _C_ERR, _C_ERR, _C_ERR, _C_ERR, _C_ERR, _C_ERR, _C_ERR,     /* 18-1F */

    _C_WSP, _C_BNG, _C_QUO, _C_SHP, _C_DOL, _C_PCT, _C_AMP, _C_APO,     /* 20-27 */
    _C_LPR, _C_RPR, _C_MUL, _C_PLS, _C_CMA, _C_MIN, _C_DOT, _C_SLH,     /* 28-2F */

    _C_DIG, _C_DIG, _C_DIG, _C_DIG, _C_DIG, _C_DIG, _C_DIG, _C_DIG,     /* 30-37 */
    _C_DIG, _C_DIG, _C_COL, _C_SMC, _C_LT , _C_EQ , _C_GT , _C_QUE,     /* 38-3F */

    _C_AT , _C_LET, _C_LET, _C_LET, _C_LET, _C_LET, _C_LET, _C_LET,     /* 40-47 */
    _C_LET, _C_LET, _C_LET, _C_LET, _C_LET, _C_LET, _C_LET, _C_LET,     /* 48-4F */

    _C_LET, _C_LET, _C_LET, _C_LET, _C_LET, _C_LET, _C_LET, _C_LET,     /* 50-57 */
    _C_LET, _C_LET, _C_LET, _C_LBR, _C_BSL, _C_RBR, _C_XOR, _C_USC,     /* 58-5F */

    _C_BKQ, _C_LET, _C_LET, _C_LET, _C_LET, _C_LET, _C_LET, _C_LET,     /* 60-67 */
    _C_LET, _C_LET, _C_LET, _C_LET, _C_LET, _C_LET, _C_LET, _C_LET,     /* 68-6F */

    _C_LET, _C_LET, _C_LET, _C_LET, _C_LET, _C_LET, _C_LET, _C_LET,     /* 70-77 */
    _C_LET, _C_LET, _C_LET, _C_LC , _C_BAR, _C_RC , _C_TIL, _C_ERR,     /* 78-7F */
};

typedef struct
{
    OLECHAR chStart;
    OLECHAR chFinish;

} oldCharTypesRangeStruct;

static const int cOldDigits = 156;
static const oldCharTypesRangeStruct oldDigits[] = {
    {   688,   734 }, {   736,   745 }, {   768,   837 }, {   864,   865 }, {   884,   885 },
    {   890,   890 }, {   900,   901 }, {  1154,  1158 }, {  1369,  1369 }, {  1425,  1441 },
    {  1443,  1465 }, {  1467,  1469 }, {  1471,  1471 }, {  1473,  1474 }, {  1476,  1476 },
    {  1600,  1600 }, {  1611,  1618 }, {  1648,  1648 }, {  1750,  1773 }, {  2305,  2307 },
    {  2364,  2381 }, {  2384,  2388 }, {  2402,  2403 }, {  2433,  2435 }, {  2492,  2492 },
    {  2494,  2500 }, {  2503,  2504 }, {  2507,  2509 }, {  2519,  2519 }, {  2530,  2531 },
    {  2546,  2554 }, {  2562,  2562 }, {  2620,  2620 }, {  2622,  2626 }, {  2631,  2632 },
    {  2635,  2637 }, {  2672,  2676 }, {  2689,  2691 }, {  2748,  2757 }, {  2759,  2761 },
    {  2763,  2765 }, {  2768,  2768 }, {  2817,  2819 }, {  2876,  2883 }, {  2887,  2888 },
    {  2891,  2893 }, {  2902,  2903 }, {  2928,  2928 }, {  2946,  2947 }, {  3006,  3010 },
    {  3014,  3016 }, {  3018,  3021 }, {  3031,  3031 }, {  3056,  3058 }, {  3073,  3075 },
    {  3134,  3140 }, {  3142,  3144 }, {  3146,  3149 }, {  3157,  3158 }, {  3202,  3203 },
    {  3262,  3268 }, {  3270,  3272 }, {  3274,  3277 }, {  3285,  3286 }, {  3330,  3331 },
    {  3390,  3395 }, {  3398,  3400 }, {  3402,  3405 }, {  3415,  3415 }, {  3647,  3647 },
    {  3759,  3769 }, {  3771,  3773 }, {  3776,  3780 }, {  3782,  3782 }, {  3784,  3789 },
    {  3840,  3843 }, {  3859,  3871 }, {  3882,  3897 }, {  3902,  3903 }, {  3953,  3972 },
    {  3974,  3979 }, {  8125,  8129 }, {  8141,  8143 }, {  8157,  8159 }, {  8173,  8175 },
    {  8189,  8190 }, {  8192,  8207 }, {  8232,  8238 }, {  8260,  8260 }, {  8298,  8304 },
    {  8308,  8316 }, {  8319,  8332 }, {  8352,  8364 }, {  8400,  8417 }, {  8448,  8504 },
    {  8531,  8578 }, {  8592,  8682 }, {  8704,  8945 }, {  8960,  8960 }, {  8962,  9000 },
    {  9003,  9082 }, {  9216,  9252 }, {  9280,  9290 }, {  9312,  9371 }, {  9450,  9450 },
    {  9472,  9621 }, {  9632,  9711 }, {  9728,  9747 }, {  9754,  9839 }, {  9985,  9988 },
    {  9990,  9993 }, {  9996, 10023 }, { 10025, 10059 }, { 10061, 10061 }, { 10063, 10066 },
    { 10070, 10070 }, { 10072, 10078 }, { 10081, 10087 }, { 10102, 10132 }, { 10136, 10159 },
    { 10161, 10174 }, { 12292, 12292 }, { 12294, 12294 }, { 12306, 12307 }, { 12320, 12335 },
    { 12337, 12343 }, { 12351, 12351 }, { 12441, 12442 }, { 12688, 12703 }, { 12800, 12828 },
    { 12832, 12867 }, { 12896, 12923 }, { 12927, 12976 }, { 12992, 13003 }, { 13008, 13054 },
    { 13056, 13174 }, { 13179, 13277 }, { 13280, 13310 }, { 64286, 64286 }, { 65056, 65059 },
    { 65122, 65122 }, { 65124, 65126 }, { 65129, 65129 }, { 65136, 65138 }, { 65140, 65140 },
    { 65142, 65151 }, { 65284, 65284 }, { 65291, 65291 }, { 65308, 65310 }, { 65342, 65342 },
    { 65344, 65344 }, { 65372, 65372 }, { 65374, 65374 }, { 65440, 65440 }, { 65504, 65510 },
    { 65512, 65518 }
};

static const int cOldAlphas = 11;
static const oldCharTypesRangeStruct oldAlphas[] = {
    {   402,   402 }, {  9372,  9449 }, { 12293, 12293 }, { 12295, 12295 }, { 12443, 12446 },
    { 12540, 12542 }, { 64297, 64297 }, { 65152, 65276 }, { 65392, 65392 }, { 65438, 65439 },
    { 65533, 65533 }
};

CharTypes GetBigCharType(codepoint_t ch);
CharTypes GetBigCharTypeES6(codepoint_t ch);

CharTypeFlags GetBigCharFlags(codepoint_t ch, const Js::CharClassifier *instance);
CharTypeFlags GetBigCharFlags5(codepoint_t ch, const Js::CharClassifier *instanceh);
CharTypeFlags GetBigCharFlagsES6(codepoint_t ch, const Js::CharClassifier *instance);

BOOL doBinSearch(OLECHAR ch, const oldCharTypesRangeStruct *pRanges, int cSize)
{
    int lo = 0;
    int hi = cSize;
    int mid;

    while (lo != hi)
    {
        mid = lo + (hi - lo) / 2;
        if (pRanges[mid].chStart <= ch && ch <= pRanges[mid].chFinish)
            return true;
        if (ch < pRanges[mid].chStart)
            hi = mid;
        else
            lo = mid + 1;
    }
    return false;
}

WORD oFindOldCharType(OLECHAR ch)
{
    if ((OLECHAR) 65279 == ch)
        return C1_SPACE;

    if (doBinSearch(ch, oldAlphas, cOldAlphas))
        return C1_ALPHA;

    if (doBinSearch(ch, oldDigits, cOldDigits))
        return C1_DIGIT;

    return 0;
}

BOOL oGetCharType( DWORD dwInfoType, OLECHAR ch, LPWORD lpwCharType )
{
    BOOL res = GetStringTypeW( dwInfoType, &ch, 1, lpwCharType );
    // BOM ( 0xfeff) is recognized as GetStringTypeW as WS.
    if ((0x03FF & *lpwCharType) == 0x0200)
    {
        // Some of the char types changed for Whistler (Unicode 3.0).
        // They will return 0x0200 on Whistler, indicating a defined char
        // with no type attributes. We want to continue to support these
        // characters, so we return the Win2K (Unicode 2.1) attributes.
        // We only return the ones we care about - ALPHA for ALPHA, PUNCT
        // for PUNCT or DIGIT, and SPACE for SPACE or BLANK.
        WORD wOldCharType = oFindOldCharType(ch);
        if (0 == wOldCharType)
            return res;

        *lpwCharType = wOldCharType;
        return TRUE;
    }
    return res;
}

CharTypes GetBigCharType(codepoint_t ch, const Js::CharClassifier *instance)
{
    if(ch > 0xFFFF)
    {
        return CharTypes::_C_ERR;
    }

    OLECHAR oCh = (OLECHAR)ch;

    WORD chType;

    Assert( oCh >= 128 );
#if (_WIN32 || _WIN64) // We use the Win32 API function GetStringTypeW for Unicode char. classification
    if( oCh == 0x2028 || oCh == 0x2029 )
    {
        return _C_NWL;
    }
    if( oGetCharType( CT_CTYPE1, oCh, &chType) )
    {
        if( chType & C1_ALPHA )
            return _C_LET;
        else if( chType & (C1_SPACE|C1_BLANK) )
            return _C_WSP;
    }
#else
#warning No Unicode character support on this platform
#endif
    return _C_ERR;
}

CharTypeFlags GetBigCharFlags(codepoint_t ch, const Js::CharClassifier *instance)
{
    WORD chType;

    if(ch > 0xFFFF)
    {
        return CharTypeFlags::UnknownChar;
    }

    OLECHAR oCh = (OLECHAR)ch;
    Assert( oCh >= 128 );
#if (_WIN32 || _WIN64) // We use the Win32 API function GetStringTypeW for Unicode char. classification
    if( oCh == kchLS || oCh == kchPS )
    {
        return LineCharGroup;
    }
    if( oGetCharType( CT_CTYPE1, oCh, &chType) )
    {
        if( chType & C1_ALPHA )
            return LetterCharGroup;
        else if ( chType & (C1_DIGIT|C1_PUNCT) )
        {
            // non-ANSI digits can be used in identifiers but not in numeric constants - hence we
            // return fChId instead of kgrfchDec
            return IdChar;
        }
        else if( chType & (C1_SPACE|C1_BLANK) )
            return SpaceChar;
    }
#else
#warning No Unicode character support on this platform
#endif
    return UnknownChar;
}


CharTypeFlags GetBigCharFlags5(codepoint_t ch, const Js::CharClassifier *instance)
{
    //In ES5 the unicode <ZWNJ> and <ZWJ> could be identifier parts
    if(ch == 0x200c || ch == 0x200d)
    {
        return IdChar;
    }
    return GetBigCharFlags(ch, instance);
}

/*
 * CharClassifier implementation
 */

UnicodeGeneralCategory Js::CharClassifier::GetUnicodeCategoryFor(codepoint_t ch) const
{
    UnicodeGeneralCategory category;
    AssertMsg(this->winGlobCharApi != nullptr, "ES6 Mode 'GetUnicodeCategoryFor' must mean winGlobCharApi is initialized.");

    if(FAILED(this->winGlobCharApi->GetGeneralCategory(ch, &category)))
    {
        AssertMsg(false, "Should not fail here!");
        return UnicodeGeneralCategory::UnicodeGeneralCategory_NotAssigned;
    }

    return category;
}

CharTypes Js::CharClassifier::GetBigCharTypeES6(codepoint_t ch, const Js::CharClassifier *instance)
{
    Assert(ch > 0x7F);
    UnicodeGeneralCategory category = instance->GetUnicodeCategoryFor(ch);

    switch(category)
    {
    case UnicodeGeneralCategory::UnicodeGeneralCategory_LowercaseLetter:
    case UnicodeGeneralCategory::UnicodeGeneralCategory_UppercaseLetter:
    case UnicodeGeneralCategory::UnicodeGeneralCategory_TitlecaseLetter:
    case UnicodeGeneralCategory::UnicodeGeneralCategory_ModifierLetter:
    case UnicodeGeneralCategory::UnicodeGeneralCategory_OtherLetter:
    case UnicodeGeneralCategory::UnicodeGeneralCategory_LetterNumber:
        return CharTypes::_C_LET;

    case UnicodeGeneralCategory::UnicodeGeneralCategory_LineSeparator:
    case UnicodeGeneralCategory::UnicodeGeneralCategory_ParagraphSeparator:
        return CharTypes::_C_NWL;
    case UnicodeGeneralCategory::UnicodeGeneralCategory_SpaceSeparator:
    case UnicodeGeneralCategory::UnicodeGeneralCategory_SpacingCombiningMark:
    case UnicodeGeneralCategory::UnicodeGeneralCategory_NonspacingMark:
    case UnicodeGeneralCategory::UnicodeGeneralCategory_ConnectorPunctuation:
        return CharTypes::_C_WSP;

    case UnicodeGeneralCategory::UnicodeGeneralCategory_DecimalDigitNumber:
        return CharTypes::_C_DIG;

    case UnicodeGeneralCategory::UnicodeGeneralCategory_ClosePunctuation:
    case UnicodeGeneralCategory::UnicodeGeneralCategory_EnclosingMark:
    case UnicodeGeneralCategory::UnicodeGeneralCategory_Control:
    case UnicodeGeneralCategory::UnicodeGeneralCategory_Format:
        if (ch == 0xFEFF)
        {
            return CharTypes::_C_WSP;
        }
        // Fall through, otherwise
    case UnicodeGeneralCategory::UnicodeGeneralCategory_Surrogate:
    case UnicodeGeneralCategory::UnicodeGeneralCategory_PrivateUse:
    case UnicodeGeneralCategory::UnicodeGeneralCategory_DashPunctuation:
    case UnicodeGeneralCategory::UnicodeGeneralCategory_OpenPunctuation:
    case UnicodeGeneralCategory::UnicodeGeneralCategory_InitialQuotePunctuation:
    case UnicodeGeneralCategory::UnicodeGeneralCategory_FinalQuotePunctuation:
    case UnicodeGeneralCategory::UnicodeGeneralCategory_OtherPunctuation:
    case UnicodeGeneralCategory::UnicodeGeneralCategory_MathSymbol:
    case UnicodeGeneralCategory::UnicodeGeneralCategory_CurrencySymbol:
    case UnicodeGeneralCategory::UnicodeGeneralCategory_ModifierSymbol:
    case UnicodeGeneralCategory::UnicodeGeneralCategory_OtherSymbol:
    case UnicodeGeneralCategory::UnicodeGeneralCategory_NotAssigned:
        return CharTypes::_C_UNK;
    }

    return CharTypes::_C_UNK;
}

/*
From Unicode 6.3 http://www.unicode.org/reports/tr31/tr31-19.html

ID_Start:::
    Characters having the Unicode General_Category of uppercase letters (Lu), lowercase letters (Ll), titlecase letters (Lt), modifier letters (Lm), other letters (Lo), letter numbers (Nl), minus Pattern_Syntax and Pattern_White_Space code points, plus stability extensions. Note that "other letters" includes ideographs.
    In set notation, this is [[:L:][:Nl:]--[:Pattern_Syntax:]--[:Pattern_White_Space:]] plus stability extensions.

ID_Continue:::
    All of the above, plus characters having the Unicode General_Category of nonspacing marks (Mn), spacing combining marks (Mc), decimal number (Nd), connector punctuations (Pc), plus stability extensions, minus Pattern_Syntax and Pattern_White_Space code points.
    In set notation, this is [[:L:][:Nl:][:Mn:][:Mc:][:Nd:][:Pc:]--[:Pattern_Syntax:]--[:Pattern_White_Space:]] plus stability extensions.

These are also known simply as Identifier Characters, because they are a superset of the ID_Start characters.
*/

CharTypeFlags Js::CharClassifier::GetBigCharFlagsES6(codepoint_t ch, const Js::CharClassifier *instance)
{
    Assert(ch > 0x7F);

    UnicodeGeneralCategory category = instance->GetUnicodeCategoryFor(ch);

    switch(category)
    {
    case UnicodeGeneralCategory::UnicodeGeneralCategory_LowercaseLetter:
    case UnicodeGeneralCategory::UnicodeGeneralCategory_UppercaseLetter:
    case UnicodeGeneralCategory::UnicodeGeneralCategory_TitlecaseLetter:
    case UnicodeGeneralCategory::UnicodeGeneralCategory_ModifierLetter:
    case UnicodeGeneralCategory::UnicodeGeneralCategory_OtherLetter:
    case UnicodeGeneralCategory::UnicodeGeneralCategory_LetterNumber:
        return BigCharIsIdStartES6(ch, instance) ? CharTypeFlags::LetterCharGroup : CharTypeFlags::UnknownChar;

    case UnicodeGeneralCategory::UnicodeGeneralCategory_SpacingCombiningMark:
        return BigCharIsIdContinueES6(ch, instance) ? CharTypeFlags::IdChar : CharTypeFlags::SpaceChar;
    case UnicodeGeneralCategory::UnicodeGeneralCategory_NonspacingMark:
    case UnicodeGeneralCategory::UnicodeGeneralCategory_ConnectorPunctuation:
        return BigCharIsIdContinueES6(ch, instance) ? CharTypeFlags::IdChar : CharTypeFlags::UnknownChar;

    case UnicodeGeneralCategory::UnicodeGeneralCategory_DecimalDigitNumber:
        return BigCharIsIdContinueES6(ch, instance) ? CharTypeFlags::DecimalCharGroup : CharTypeFlags::DecimalChar;

    case UnicodeGeneralCategory::UnicodeGeneralCategory_LineSeparator:
        return CharTypeFlags::LineFeedChar;
    case UnicodeGeneralCategory::UnicodeGeneralCategory_ParagraphSeparator:
    case UnicodeGeneralCategory::UnicodeGeneralCategory_SpaceSeparator:
        return CharTypeFlags::SpaceChar;

    case UnicodeGeneralCategory::UnicodeGeneralCategory_ClosePunctuation:
    case UnicodeGeneralCategory::UnicodeGeneralCategory_EnclosingMark:
    case UnicodeGeneralCategory::UnicodeGeneralCategory_Control:
    case UnicodeGeneralCategory::UnicodeGeneralCategory_Format:
    case UnicodeGeneralCategory::UnicodeGeneralCategory_Surrogate:
    case UnicodeGeneralCategory::UnicodeGeneralCategory_PrivateUse:
    case UnicodeGeneralCategory::UnicodeGeneralCategory_DashPunctuation:
    case UnicodeGeneralCategory::UnicodeGeneralCategory_OpenPunctuation:
    case UnicodeGeneralCategory::UnicodeGeneralCategory_InitialQuotePunctuation:
    case UnicodeGeneralCategory::UnicodeGeneralCategory_FinalQuotePunctuation:
    case UnicodeGeneralCategory::UnicodeGeneralCategory_OtherPunctuation:
    case UnicodeGeneralCategory::UnicodeGeneralCategory_MathSymbol:
    case UnicodeGeneralCategory::UnicodeGeneralCategory_CurrencySymbol:
    case UnicodeGeneralCategory::UnicodeGeneralCategory_ModifierSymbol:
    case UnicodeGeneralCategory::UnicodeGeneralCategory_OtherSymbol:
    case UnicodeGeneralCategory::UnicodeGeneralCategory_NotAssigned:
        return CharTypeFlags::UnknownChar;
    }

    return CharTypeFlags::UnknownChar;
}

BOOL Js::CharClassifier::BigCharIsWhitespaceES6(codepoint_t ch, const CharClassifier *instance)
{
    Assert(ch > 0x7F);

    if (ch == 0xFEFF)
    {
        return true;
    }

    boolean toReturn = false;
    AssertMsg(instance->winGlobCharApi != nullptr, "ES6 Mode 'BigCharIsWhitespaceES6' must mean winGlobCharApi is initialized.");
    if (FAILED(instance->winGlobCharApi->IsWhitespace(ch, &toReturn)))
    {
        AssertMsg(false, "Should not fail here!");
        return toReturn;
    }

    return toReturn;
}

BOOL Js::CharClassifier::BigCharIsIdStartES6(codepoint_t codePoint, const CharClassifier *instance)
{
    Assert(codePoint > 0x7F);

    boolean toReturn = false;
    AssertMsg(instance->winGlobCharApi != nullptr, "ES6 Mode 'BigCharIsIdStartES6' must mean winGlobCharApi is initialized.");
    if (FAILED(instance->winGlobCharApi->IsIdStart(codePoint, &toReturn)))
    {
        AssertMsg(false, "Should not fail here!");
        return toReturn;
    }

    return toReturn;
}

BOOL Js::CharClassifier::BigCharIsIdContinueES6(codepoint_t codePoint, const CharClassifier *instance)
{
    Assert(codePoint > 0x7F);

    if (codePoint == '$' || codePoint == '_' || codePoint == 0x200C /* Zero-width non-joiner */ || codePoint == 0x200D /* Zero-width joiner */)
    {
        return true;
    }

    boolean toReturn = false;
    AssertMsg(instance->winGlobCharApi != nullptr, "ES6 Mode 'BigCharIsIdContinueES6' must mean winGlobCharApi is initialized.");
    if (FAILED(instance->winGlobCharApi->IsIdContinue(codePoint, &toReturn)))
    {
        AssertMsg(false, "Should not fail here!");
        return toReturn;
    }

    return toReturn;
}

template <bool isBigChar>
BOOL Js::CharClassifier::IsWhiteSpaceFast(codepoint_t ch) const
{
    Assert(isBigChar ? ch > 0x7F : ch < 0x80);
    return isBigChar ? this->bigCharIsWhitespaceFunc(ch, this) : (charFlags[ch] & CharTypeFlags::SpaceChar);
}

BOOL Js::CharClassifier::IsBiDirectionalChar(codepoint_t ch) const
{
    //From http://www.unicode.org/reports/tr9/#Directional_Formatting_Codes
    switch (ch)
    {
    case 0x202A: //LEFT-TO-RIGHT EMBEDDING Treat the following text as embedded left-to-right
    case 0x202B: //RIGHT-TO-LEFT EMBEDDING Treat the following text as embedded right-to-left.
    case 0x202D: //LEFT-TO-RIGHT OVERRIDE Force following characters to be treated as strong left-to-right characters.
    case 0x202E: //RIGHT-TO-LEFT OVERRIDE Force following characters to be treated as strong right-to-left characters.
    case 0x202C: //POP DIRECTIONAL FORMATTING End the scope of the last LRE, RLE, RLO, or LRO.
    case 0x2066: //LEFT-TO-RIGHT ISOLATE Treat the following text as isolated and left-to-right.
    case 0x2067: //RIGHT-TO-LEFT ISOLATE Treat the following text as isolated and right-to-left.
    case 0x2068: //FIRST STRONG ISOLATE Treat the following text as isolated and in the direction of its first strong directional character that is not inside a nested isolate.
    case 0x2069: //POP DIRECTIONAL ISOLATE End the scope of the last LRI, RLI, or FSI.
    case 0x200E: //LEFT-TO-RIGHT MARK Left-to-right zero-width character
    case 0x200F: //RIGHT-TO-LEFT MARK Right-to-left zero-width non-Arabic character
    case 0x061C: //ARABIC LETTER MARK Right-to-left zero-width Arabic character
        return TRUE;
    default:
        return FALSE;
    }
}

template<bool isBigChar>
BOOL Js::CharClassifier::IsIdStartFast(codepoint_t ch) const
{
    Assert(isBigChar ? ch > 0x7F : ch < 0x80);
    return isBigChar ? this->bigCharIsIdStartFunc(ch, this) : (charFlags[ch] & CharTypeFlags::IdLeadChar);
}
template<bool isBigChar>
BOOL Js::CharClassifier::IsIdContinueFast(codepoint_t ch) const
{
    Assert(isBigChar ? ch > 0x7F : ch < 0x80);
    return isBigChar ? this->bigCharIsIdContinueFunc(ch, this) : (charFlags[ch] & CharTypeFlags::IdChar);
}

Js::CharClassifier::CharClassifier(ScriptContext * scriptContext)
{
    CharClassifierModes overallMode = (CONFIG_FLAG(ES6Unicode)) ? CharClassifierModes::ES6 : CharClassifierModes::ES5;
    bool codePointSupport = overallMode == CharClassifierModes::ES6;
    bool isES6UnicodeVerboseEnabled = scriptContext->GetConfig()->IsES6UnicodeVerboseEnabled();

    initClassifier(scriptContext, overallMode, overallMode, overallMode, codePointSupport, isES6UnicodeVerboseEnabled, CharClassifierModes::ES6); // no fallback for chk
}

void Js::CharClassifier::initClassifier(ScriptContext * scriptContext, CharClassifierModes identifierSupport,
                                        CharClassifierModes whiteSpaceSupport, CharClassifierModes generalCharClassificationSupport, bool codePointSupport, bool isES6UnicodeVerboseEnabled, CharClassifierModes es6FallbackMode)
{
    bool es6Supported = true;
    bool es6ModeNeeded = identifierSupport == CharClassifierModes::ES6 || whiteSpaceSupport == CharClassifierModes::ES6 || generalCharClassificationSupport == CharClassifierModes::ES6;

#ifdef ENABLE_ES6_CHAR_CLASSIFIER
    ThreadContext* threadContext = scriptContext->GetThreadContext();
    Js::WindowsGlobalizationAdapter* globalizationAdapter = threadContext->GetWindowsGlobalizationAdapter();
    Js::DelayLoadWindowsGlobalization* globLibrary = threadContext->GetWindowsGlobalizationLibrary();
    if (es6ModeNeeded)
    {
        HRESULT hr = globalizationAdapter->EnsureDataTextObjectsInitialized(globLibrary);
        if (FAILED(hr))
        {
            AssertMsg(false, "Failed to initialize COM interfaces, verify correct version of globalization dll is used.");
            JavascriptError::MapAndThrowError(scriptContext, hr);
        }

        this->winGlobCharApi = globalizationAdapter->GetUnicodeStatics();
        if (this->winGlobCharApi == nullptr)
        {
            // No fallback mode, then assert
            if (es6FallbackMode == CharClassifierModes::ES6)
            {
                AssertMsg(false, "Windows::Data::Text::IUnicodeCharactersStatics not initialized");
                //Fallback to ES5 just in case for fre builds.
                es6FallbackMode = CharClassifierModes::ES5;
            }
            if (isES6UnicodeVerboseEnabled)
            {
                Output::Print(L"Windows::Data::Text::IUnicodeCharactersStatics not initialized\r\n");
            }
            //Default to non-es6
            es6Supported = false;
        }
    }
#else
    es6Supported = false;
    es6FallbackMode = CharClassifierModes::ES5;
#endif

    if (es6ModeNeeded && !es6Supported)
    {
        identifierSupport = identifierSupport == CharClassifierModes::ES6 ? es6FallbackMode : identifierSupport;
        whiteSpaceSupport = whiteSpaceSupport == CharClassifierModes::ES6 ? es6FallbackMode : whiteSpaceSupport;
        generalCharClassificationSupport = generalCharClassificationSupport == CharClassifierModes::ES6 ? es6FallbackMode : generalCharClassificationSupport;
    }


    bigCharIsIdStartFunc = identifierSupport == CharClassifierModes::ES6 ? &CharClassifier::BigCharIsIdStartES6 : &CharClassifier::BigCharIsIdStartDefault;
    bigCharIsIdContinueFunc = identifierSupport == CharClassifierModes::ES6 ? &CharClassifier::BigCharIsIdContinueES6 : &CharClassifier::BigCharIsIdContinueDefault;
    bigCharIsWhitespaceFunc = whiteSpaceSupport == CharClassifierModes::ES6 ? &CharClassifier::BigCharIsWhitespaceES6 : &CharClassifier::BigCharIsWhitespaceDefault;

    skipWhiteSpaceFunc = codePointSupport ? &CharClassifier::SkipWhiteSpaceSurrogate : &CharClassifier::SkipWhiteSpaceNonSurrogate;
    skipWhiteSpaceStartEndFunc = codePointSupport ? &CharClassifier::SkipWhiteSpaceSurrogateStartEnd : &CharClassifier::SkipWhiteSpaceNonSurrogateStartEnd;

    skipIdentifierFunc = codePointSupport ? &CharClassifier::SkipIdentifierSurrogate : &CharClassifier::SkipIdentifierNonSurrogate;
    skipIdentifierStartEndFunc = codePointSupport ? &CharClassifier::SkipIdentifierSurrogateStartEnd : &CharClassifier::SkipIdentifierNonSurrogateStartEnd;

    if (generalCharClassificationSupport == CharClassifierModes::ES6)
    {
        getBigCharTypeFunc = &CharClassifier::GetBigCharTypeES6;
        getBigCharFlagsFunc = &CharClassifier::GetBigCharFlagsES6;
    }
    else if (generalCharClassificationSupport == CharClassifierModes::ES5)
    {
        getBigCharTypeFunc = &GetBigCharType;
        getBigCharFlagsFunc = &GetBigCharFlags5;
    }
    else
    {
        getBigCharTypeFunc = &GetBigCharType;
        getBigCharFlagsFunc = &GetBigCharFlags;
    }
}

const OLECHAR* Js::CharClassifier::SkipWhiteSpaceNonSurrogate(LPCOLESTR psz, const CharClassifier *instance)
{
    for ( ; instance->IsWhiteSpace(*psz); psz++)
    {
    }
    return psz;
}

const OLECHAR* Js::CharClassifier::SkipWhiteSpaceNonSurrogateStartEnd(_In_reads_(pStrEnd - pStr) LPCOLESTR pStr, _In_ LPCOLESTR pStrEnd, const CharClassifier *instance)
{
    for ( ; instance->IsWhiteSpace(*pStr) && pStr < pStrEnd; pStr++)
    {
    }
    return pStr;
}

const OLECHAR* Js::CharClassifier::SkipIdentifierNonSurrogate(LPCOLESTR psz, const CharClassifier *instance)
{
    if (!instance->IsIdStart(*psz))
    {
        return psz;
    }

    for (psz++; instance->IsIdContinue(*psz); psz++)
    {
    }

    return psz;
}

const LPCUTF8 Js::CharClassifier::SkipIdentifierNonSurrogateStartEnd(LPCUTF8 psz, LPCUTF8 end, const CharClassifier *instance)
{
    utf8::DecodeOptions options = utf8::doAllowThreeByteSurrogates;

    LPCUTF8 p = psz;

    if (!instance->IsIdStart(utf8::Decode(p, end, options)))
    {
        return psz;
    }

    psz = p;

    while (instance->IsIdContinue(utf8::Decode(p, end, options)))
    {
        psz = p;
    }

    return psz;
}

const OLECHAR* Js::CharClassifier::SkipWhiteSpaceSurrogate(LPCOLESTR psz, const CharClassifier *instance)
{
    wchar_t currentChar = 0x0;

    // Slow path is to check for a surrogate each iteration.
    // There is no new surrogate whitespaces as of yet, however, might be in the future, so surrogates still need to be checked
    // So, based on that, best way is to hit the slow path if the current character is not a whitespace in [0, FFFF];
    while((currentChar = *psz) != '\0')
    {
        if (!instance->IsWhiteSpace(*psz))
        {
            if (Js::NumberUtilities::IsSurrogateLowerPart(currentChar) && Js::NumberUtilities::IsSurrogateUpperPart(*(psz + 1)))
            {
                if (instance->IsWhiteSpace(Js::NumberUtilities::SurrogatePairAsCodePoint(currentChar, *(psz + 1))))
                {
                    psz += 2;
                    continue;
                }
            }

            // Above case failed, so we have reached the last whitespace
            return psz;
        }

        psz++;
    }

    return psz;
}

const OLECHAR* Js::CharClassifier::SkipWhiteSpaceSurrogateStartEnd(_In_reads_(pStrEnd - pStr) LPCOLESTR pStr, _In_ LPCOLESTR pStrEnd, const CharClassifier *instance)
{
    wchar_t currentChar = 0x0;

    // Same reasoning as above
    while(pStr < pStrEnd && (currentChar = *pStr) != '\0')
    {
        if (!instance->IsWhiteSpace(currentChar))
        {
            if (Js::NumberUtilities::IsSurrogateLowerPart(currentChar) && (pStr + 1) < pStrEnd && Js::NumberUtilities::IsSurrogateUpperPart(*(pStr + 1)))
            {
                if (instance->IsWhiteSpace(Js::NumberUtilities::SurrogatePairAsCodePoint(currentChar, *(pStr + 1))))
                {
                    pStr += 2;
                    continue;
                }
            }

            // Above case failed, so we have reached the last whitespace
            return pStr;
        }

        pStr++;
    }

    return pStr;
}

const OLECHAR* Js::CharClassifier::SkipIdentifierSurrogate(LPCOLESTR psz, const CharClassifier *instance)
{
    // Similar reasoning to above, however we do have surrogate identifiers, but less likely to occur in code.
    wchar_t currentChar = *psz;

    if (!instance->IsIdStart(currentChar))
    {
        if (Js::NumberUtilities::IsSurrogateLowerPart(currentChar) && Js::NumberUtilities::IsSurrogateUpperPart(*(psz + 1))
            && instance->IsIdStart(Js::NumberUtilities::SurrogatePairAsCodePoint(currentChar, *(psz + 1))))
        {
            // For the extra surrogate char
            psz ++;
        }
        else
        {
            return psz;
        }
    }

    psz++;

    while((currentChar = *psz) != '\0')
    {
        if (!instance->IsIdContinue(*psz))
        {
            if (Js::NumberUtilities::IsSurrogateLowerPart(currentChar) && Js::NumberUtilities::IsSurrogateUpperPart(*(psz + 1)))
            {
                if (instance->IsIdContinue(Js::NumberUtilities::SurrogatePairAsCodePoint(currentChar, *(psz + 1))))
                {
                    psz += 2;
                    continue;
                }
            }

            // Above case failed, so we have reached the last IDContinue
            return psz;
        }

        psz++;
    }

    return psz;
}

const LPCUTF8 Js::CharClassifier::SkipIdentifierSurrogateStartEnd(LPCUTF8 psz, LPCUTF8 end, const CharClassifier *instance)
{

    LPCUTF8 currentPosition = psz;
    utf8::DecodeOptions options = utf8::doAllowThreeByteSurrogates;

    // Similar reasoning to above, however we do have surrogate identifiers, but less likely to occur in code.
    codepoint_t currentChar = utf8::Decode(currentPosition, end, options);

    if (options & utf8::doSecondSurrogatePair)
    {
        currentChar = Js::NumberUtilities::SurrogatePairAsCodePoint(currentChar, utf8::Decode(currentPosition, end, options));
    }

    if (!instance->IsIdStart(currentChar))
    {
        return psz;
    }

    psz = currentPosition;

    // Slow path is to check for a surrogate each iteration.
    // There is no new surrogate whitespaces as of yet, however, might be in the future, so surrogates still need to be checked
    // So, based on that, best way is to hit the slow path if the current character is not a whitespace in [0, FFFF];
    while((currentChar = utf8::Decode(currentPosition, end, options)) != '\0')
    {
        if (options & utf8::doSecondSurrogatePair)
        {
            currentChar = Js::NumberUtilities::SurrogatePairAsCodePoint(currentChar, utf8::Decode(currentPosition, end, options));
        }

        if (!instance->IsIdContinue(currentChar))
        {
            return psz;
        }

        psz = currentPosition;
    }

    return psz;
}

CharTypes Js::CharClassifier::GetCharType(codepoint_t ch) const
{
    return FBigChar(ch) ? getBigCharTypeFunc(ch, this) : charTypes[ch];
}

CharTypeFlags Js::CharClassifier::GetCharFlags(codepoint_t ch) const
{
    return FBigChar(ch) ? getBigCharFlagsFunc(ch, this) : charFlags[ch];
}
