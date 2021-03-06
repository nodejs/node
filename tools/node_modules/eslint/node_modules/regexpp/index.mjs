/*! @author Toru Nagashima <https://github.com/mysticatea> */


var ast = /*#__PURE__*/Object.freeze({

});

let largeIdStartRanges = undefined;
let largeIdContinueRanges = undefined;
function isIdStart(cp) {
    if (cp < 0x41)
        return false;
    if (cp < 0x5b)
        return true;
    if (cp < 0x61)
        return false;
    if (cp < 0x7b)
        return true;
    return isLargeIdStart(cp);
}
function isIdContinue(cp) {
    if (cp < 0x30)
        return false;
    if (cp < 0x3a)
        return true;
    if (cp < 0x41)
        return false;
    if (cp < 0x5b)
        return true;
    if (cp === 0x5f)
        return true;
    if (cp < 0x61)
        return false;
    if (cp < 0x7b)
        return true;
    return isLargeIdStart(cp) || isLargeIdContinue(cp);
}
function isLargeIdStart(cp) {
    return isInRange(cp, largeIdStartRanges || (largeIdStartRanges = initLargeIdStartRanges()));
}
function isLargeIdContinue(cp) {
    return isInRange(cp, largeIdContinueRanges ||
        (largeIdContinueRanges = initLargeIdContinueRanges()));
}
function initLargeIdStartRanges() {
    return restoreRanges("170 0 11 0 5 0 6 22 2 30 2 457 5 11 15 4 8 0 2 0 130 4 2 1 3 3 2 0 7 0 2 2 2 0 2 19 2 82 2 138 9 165 2 37 3 0 7 40 72 26 5 3 46 42 36 1 2 98 2 0 16 1 8 1 11 2 3 0 17 0 2 29 30 88 12 0 25 32 10 1 5 0 6 21 5 0 10 0 4 0 24 24 8 10 54 20 2 17 61 53 4 0 19 0 8 9 16 15 5 7 3 1 3 21 2 6 2 0 4 3 4 0 17 0 14 1 2 2 15 1 11 0 9 5 5 1 3 21 2 6 2 1 2 1 2 1 32 3 2 0 20 2 17 8 2 2 2 21 2 6 2 1 2 4 4 0 19 0 16 1 24 0 12 7 3 1 3 21 2 6 2 1 2 4 4 0 31 1 2 2 16 0 18 0 2 5 4 2 2 3 4 1 2 0 2 1 4 1 4 2 4 11 23 0 53 7 2 2 2 22 2 15 4 0 27 2 6 1 31 0 5 7 2 2 2 22 2 9 2 4 4 0 33 0 2 1 16 1 18 8 2 2 2 40 3 0 17 0 6 2 9 2 25 5 6 17 4 23 2 8 2 0 3 6 59 47 2 1 13 6 59 1 2 0 2 4 2 23 2 0 2 9 2 1 10 0 3 4 2 0 22 3 33 0 64 7 2 35 28 4 116 42 21 0 17 5 5 3 4 0 4 1 8 2 5 12 13 0 18 37 2 0 6 0 3 42 2 332 2 3 3 6 2 0 2 3 3 40 2 3 3 32 2 3 3 6 2 0 2 3 3 14 2 56 2 3 3 66 38 15 17 85 3 5 4 619 3 16 2 25 6 74 4 10 8 12 2 3 15 17 15 17 15 12 2 2 16 51 36 0 5 0 68 88 8 40 2 0 6 69 11 30 50 29 3 4 12 43 5 25 55 22 10 52 83 0 94 46 18 6 56 29 14 1 11 43 27 35 42 2 11 35 3 8 8 42 3 2 42 3 2 5 2 1 4 0 6 191 65 277 3 5 3 37 3 5 3 7 2 0 2 0 2 0 2 30 3 52 2 6 2 0 4 2 2 6 4 3 3 5 5 12 6 2 2 6 117 0 14 0 17 12 102 0 5 0 3 9 2 0 3 5 7 0 2 0 2 0 2 15 3 3 6 4 5 0 18 40 2680 46 2 46 2 132 7 3 4 1 13 37 2 0 6 0 3 55 8 0 17 22 10 6 2 6 2 6 2 6 2 6 2 6 2 6 2 6 551 2 26 8 8 4 3 4 5 85 5 4 2 89 2 3 6 42 2 93 18 31 49 15 513 6591 65 20988 4 1164 68 45 3 268 4 15 11 1 21 46 17 30 3 79 40 8 3 102 3 52 3 8 43 12 2 2 2 3 2 22 30 51 15 49 63 5 4 0 2 1 12 27 11 22 26 28 8 46 29 0 17 4 2 9 11 4 2 40 24 2 2 7 21 22 4 0 4 49 2 0 4 1 3 4 3 0 2 0 25 2 3 10 8 2 13 5 3 5 3 5 10 6 2 6 2 42 2 13 7 114 30 11171 13 22 5 48 8453 365 3 105 39 6 13 4 6 0 2 9 2 12 2 4 2 0 2 1 2 1 2 107 34 362 19 63 3 53 41 11 117 4 2 134 37 25 7 25 12 88 4 5 3 5 3 5 3 2 36 11 2 25 2 18 2 1 2 14 3 13 35 122 70 52 268 28 4 48 48 31 14 29 6 37 11 29 3 35 5 7 2 4 43 157 19 35 5 35 5 39 9 51 157 310 10 21 11 7 153 5 3 0 2 43 2 1 4 0 3 22 11 22 10 30 66 18 2 1 11 21 11 25 71 55 7 1 65 0 16 3 2 2 2 28 43 28 4 28 36 7 2 27 28 53 11 21 11 18 14 17 111 72 56 50 14 50 14 35 349 41 7 1 79 28 11 0 9 21 107 20 28 22 13 52 76 44 33 24 27 35 30 0 3 0 9 34 4 0 13 47 15 3 22 0 2 0 36 17 2 24 85 6 2 0 2 3 2 14 2 9 8 46 39 7 3 1 3 21 2 6 2 1 2 4 4 0 19 0 13 4 159 52 19 3 21 2 31 47 21 1 2 0 185 46 42 3 37 47 21 0 60 42 14 0 72 26 230 43 117 63 32 7 3 0 3 7 2 1 2 23 16 0 2 0 95 7 3 38 17 0 2 0 29 0 11 39 8 0 22 0 12 45 20 0 35 56 264 8 2 36 18 0 50 29 113 6 2 1 2 37 22 0 26 5 2 1 2 31 15 0 328 18 190 0 80 921 103 110 18 195 2749 1070 4050 582 8634 568 8 30 114 29 19 47 17 3 32 20 6 18 689 63 129 74 6 0 67 12 65 1 2 0 29 6135 9 1237 43 8 8952 286 50 2 18 3 9 395 2309 106 6 12 4 8 8 9 5991 84 2 70 2 1 3 0 3 1 3 3 2 11 2 0 2 6 2 64 2 3 3 7 2 6 2 27 2 3 2 4 2 0 4 6 2 339 3 24 2 24 2 30 2 24 2 30 2 24 2 30 2 24 2 30 2 24 2 7 2357 44 11 6 17 0 370 43 1301 196 60 67 8 0 1205 3 2 26 2 1 2 0 3 0 2 9 2 3 2 0 2 0 7 0 5 0 2 0 2 0 2 2 2 1 2 0 3 0 2 0 2 0 2 0 2 0 2 1 2 0 3 3 2 6 2 3 2 3 2 0 2 9 2 16 6 2 2 4 2 16 4421 42717 35 4148 12 221 3 5761 15 7472 3104 541 1507 4938");
}
function initLargeIdContinueRanges() {
    return restoreRanges("183 0 585 111 24 0 252 4 266 44 2 0 2 1 2 1 2 0 73 10 49 30 7 0 102 6 3 5 3 1 2 3 3 9 24 0 31 26 92 10 16 9 34 8 10 0 25 3 2 8 2 2 2 4 44 2 120 14 2 32 55 2 2 17 2 6 11 1 3 9 18 2 57 0 2 6 3 1 3 2 10 0 11 1 3 9 15 0 3 2 57 0 2 4 5 1 3 2 4 0 21 11 4 0 12 2 57 0 2 7 2 2 2 2 21 1 3 9 11 5 2 2 57 0 2 6 3 1 3 2 8 2 11 1 3 9 19 0 60 4 4 2 2 3 10 0 15 9 17 4 58 6 2 2 2 3 8 1 12 1 3 9 18 2 57 0 2 6 2 2 2 3 8 1 12 1 3 9 17 3 56 1 2 6 2 2 2 3 10 0 11 1 3 9 18 2 71 0 5 5 2 0 2 7 7 9 3 1 62 0 3 6 13 7 2 9 88 0 3 8 12 5 3 9 63 1 7 9 12 0 2 0 2 0 5 1 50 19 2 1 6 10 2 35 10 0 101 19 2 9 13 3 5 2 2 2 3 6 4 3 14 11 2 14 704 2 10 8 929 2 30 2 30 1 31 1 65 31 10 0 3 9 34 2 3 9 144 0 119 11 5 11 11 9 129 10 61 4 58 9 2 28 3 10 7 9 23 13 2 1 64 4 48 16 12 9 18 8 13 2 31 12 3 9 45 13 49 19 9 9 7 9 119 2 2 20 5 0 7 0 3 2 199 57 2 4 576 1 20 0 124 12 5 0 4 11 3071 2 142 0 97 31 555 5 106 1 30086 9 70 0 5 9 33 1 81 1 273 0 4 0 5 0 24 4 5 0 84 1 51 17 11 9 7 17 14 10 29 7 26 12 45 3 48 13 16 9 12 0 11 9 48 13 13 0 9 1 3 9 34 2 51 0 2 2 3 1 6 1 2 0 42 4 6 1 237 7 2 1 3 9 20261 0 738 15 17 15 4 1 25 2 193 9 38 0 702 0 227 0 150 4 294 9 1368 2 2 1 6 3 41 2 5 0 166 1 574 3 9 9 370 1 154 10 176 2 54 14 32 9 16 3 46 10 54 9 7 2 37 13 2 9 6 1 45 0 13 2 49 13 9 3 2 11 83 11 7 0 161 11 6 9 7 3 56 1 2 6 3 1 3 2 10 0 11 1 3 6 4 4 193 17 10 9 5 0 82 19 13 9 214 6 3 8 28 1 83 16 16 9 82 12 9 9 84 14 5 9 243 14 166 9 71 5 2 1 3 3 2 0 2 1 13 9 120 6 3 6 4 0 29 9 41 6 2 3 9 0 10 10 47 15 406 7 2 7 17 9 57 21 2 13 123 5 4 0 2 1 2 6 2 0 9 9 49 4 2 1 2 4 9 9 330 3 19306 9 135 4 60 6 26 9 1014 0 2 54 8 3 82 0 12 1 19628 1 5319 4 4 5 9 7 3 6 31 3 149 2 1418 49 513 54 5 49 9 0 15 0 23 4 2 14 1361 6 2 16 3 6 2 1 2 4 262 6 10 9 419 13 1495 6 110 6 6 9 4759 9 787719 239");
}
function isInRange(cp, ranges) {
    let l = 0, r = (ranges.length / 2) | 0, i = 0, min = 0, max = 0;
    while (l < r) {
        i = ((l + r) / 2) | 0;
        min = ranges[2 * i];
        max = ranges[2 * i + 1];
        if (cp < min) {
            r = i;
        }
        else if (cp > max) {
            l = i + 1;
        }
        else {
            return true;
        }
    }
    return false;
}
function restoreRanges(data) {
    let last = 0;
    return data.split(" ").map(s => (last += parseInt(s, 10) | 0));
}

class DataSet {
    constructor(raw2018, raw2019, raw2020) {
        this._raw2018 = raw2018;
        this._raw2019 = raw2019;
        this._raw2020 = raw2020;
    }
    get es2018() {
        return (this._set2018 || (this._set2018 = new Set(this._raw2018.split(" "))));
    }
    get es2019() {
        return (this._set2019 || (this._set2019 = new Set(this._raw2019.split(" "))));
    }
    get es2020() {
        return (this._set2020 || (this._set2020 = new Set(this._raw2020.split(" "))));
    }
}
const gcNameSet = new Set(["General_Category", "gc"]);
const scNameSet = new Set(["Script", "Script_Extensions", "sc", "scx"]);
const gcValueSets = new DataSet("C Cased_Letter Cc Cf Close_Punctuation Cn Co Combining_Mark Connector_Punctuation Control Cs Currency_Symbol Dash_Punctuation Decimal_Number Enclosing_Mark Final_Punctuation Format Initial_Punctuation L LC Letter Letter_Number Line_Separator Ll Lm Lo Lowercase_Letter Lt Lu M Mark Math_Symbol Mc Me Mn Modifier_Letter Modifier_Symbol N Nd Nl No Nonspacing_Mark Number Open_Punctuation Other Other_Letter Other_Number Other_Punctuation Other_Symbol P Paragraph_Separator Pc Pd Pe Pf Pi Po Private_Use Ps Punctuation S Sc Separator Sk Sm So Space_Separator Spacing_Mark Surrogate Symbol Titlecase_Letter Unassigned Uppercase_Letter Z Zl Zp Zs cntrl digit punct", "", "");
const scValueSets = new DataSet("Adlam Adlm Aghb Ahom Anatolian_Hieroglyphs Arab Arabic Armenian Armi Armn Avestan Avst Bali Balinese Bamu Bamum Bass Bassa_Vah Batak Batk Beng Bengali Bhaiksuki Bhks Bopo Bopomofo Brah Brahmi Brai Braille Bugi Buginese Buhd Buhid Cakm Canadian_Aboriginal Cans Cari Carian Caucasian_Albanian Chakma Cham Cher Cherokee Common Copt Coptic Cprt Cuneiform Cypriot Cyrillic Cyrl Deseret Deva Devanagari Dsrt Dupl Duployan Egyp Egyptian_Hieroglyphs Elba Elbasan Ethi Ethiopic Geor Georgian Glag Glagolitic Gonm Goth Gothic Gran Grantha Greek Grek Gujarati Gujr Gurmukhi Guru Han Hang Hangul Hani Hano Hanunoo Hatr Hatran Hebr Hebrew Hira Hiragana Hluw Hmng Hung Imperial_Aramaic Inherited Inscriptional_Pahlavi Inscriptional_Parthian Ital Java Javanese Kaithi Kali Kana Kannada Katakana Kayah_Li Khar Kharoshthi Khmer Khmr Khoj Khojki Khudawadi Knda Kthi Lana Lao Laoo Latin Latn Lepc Lepcha Limb Limbu Lina Linb Linear_A Linear_B Lisu Lyci Lycian Lydi Lydian Mahajani Mahj Malayalam Mand Mandaic Mani Manichaean Marc Marchen Masaram_Gondi Meetei_Mayek Mend Mende_Kikakui Merc Mero Meroitic_Cursive Meroitic_Hieroglyphs Miao Mlym Modi Mong Mongolian Mro Mroo Mtei Mult Multani Myanmar Mymr Nabataean Narb Nbat New_Tai_Lue Newa Nko Nkoo Nshu Nushu Ogam Ogham Ol_Chiki Olck Old_Hungarian Old_Italic Old_North_Arabian Old_Permic Old_Persian Old_South_Arabian Old_Turkic Oriya Orkh Orya Osage Osge Osma Osmanya Pahawh_Hmong Palm Palmyrene Pau_Cin_Hau Pauc Perm Phag Phags_Pa Phli Phlp Phnx Phoenician Plrd Prti Psalter_Pahlavi Qaac Qaai Rejang Rjng Runic Runr Samaritan Samr Sarb Saur Saurashtra Sgnw Sharada Shavian Shaw Shrd Sidd Siddham SignWriting Sind Sinh Sinhala Sora Sora_Sompeng Soyo Soyombo Sund Sundanese Sylo Syloti_Nagri Syrc Syriac Tagalog Tagb Tagbanwa Tai_Le Tai_Tham Tai_Viet Takr Takri Tale Talu Tamil Taml Tang Tangut Tavt Telu Telugu Tfng Tglg Thaa Thaana Thai Tibetan Tibt Tifinagh Tirh Tirhuta Ugar Ugaritic Vai Vaii Wara Warang_Citi Xpeo Xsux Yi Yiii Zanabazar_Square Zanb Zinh Zyyy", "Dogr Dogra Gong Gunjala_Gondi Hanifi_Rohingya Maka Makasar Medefaidrin Medf Old_Sogdian Rohg Sogd Sogdian Sogo", "Elym Elymaic Hmnp Nand Nandinagari Nyiakeng_Puachue_Hmong Wancho Wcho");
const binPropertySets = new DataSet("AHex ASCII ASCII_Hex_Digit Alpha Alphabetic Any Assigned Bidi_C Bidi_Control Bidi_M Bidi_Mirrored CI CWCF CWCM CWKCF CWL CWT CWU Case_Ignorable Cased Changes_When_Casefolded Changes_When_Casemapped Changes_When_Lowercased Changes_When_NFKC_Casefolded Changes_When_Titlecased Changes_When_Uppercased DI Dash Default_Ignorable_Code_Point Dep Deprecated Dia Diacritic Emoji Emoji_Component Emoji_Modifier Emoji_Modifier_Base Emoji_Presentation Ext Extender Gr_Base Gr_Ext Grapheme_Base Grapheme_Extend Hex Hex_Digit IDC IDS IDSB IDST IDS_Binary_Operator IDS_Trinary_Operator ID_Continue ID_Start Ideo Ideographic Join_C Join_Control LOE Logical_Order_Exception Lower Lowercase Math NChar Noncharacter_Code_Point Pat_Syn Pat_WS Pattern_Syntax Pattern_White_Space QMark Quotation_Mark RI Radical Regional_Indicator SD STerm Sentence_Terminal Soft_Dotted Term Terminal_Punctuation UIdeo Unified_Ideograph Upper Uppercase VS Variation_Selector White_Space XIDC XIDS XID_Continue XID_Start space", "Extended_Pictographic", "");
function isValidUnicodeProperty(version, name, value) {
    if (gcNameSet.has(name)) {
        return version >= 2018 && gcValueSets.es2018.has(value);
    }
    if (scNameSet.has(name)) {
        return ((version >= 2018 && scValueSets.es2018.has(value)) ||
            (version >= 2019 && scValueSets.es2019.has(value)) ||
            (version >= 2020 && scValueSets.es2020.has(value)));
    }
    return false;
}
function isValidLoneUnicodeProperty(version, value) {
    return ((version >= 2018 && binPropertySets.es2018.has(value)) ||
        (version >= 2019 && binPropertySets.es2019.has(value)));
}

const Backspace = 0x08;
const CharacterTabulation = 0x09;
const LineFeed = 0x0a;
const LineTabulation = 0x0b;
const FormFeed = 0x0c;
const CarriageReturn = 0x0d;
const ExclamationMark = 0x21;
const DollarSign = 0x24;
const LeftParenthesis = 0x28;
const RightParenthesis = 0x29;
const Asterisk = 0x2a;
const PlusSign = 0x2b;
const Comma = 0x2c;
const HyphenMinus = 0x2d;
const FullStop = 0x2e;
const Solidus = 0x2f;
const DigitZero = 0x30;
const DigitOne = 0x31;
const DigitSeven = 0x37;
const DigitNine = 0x39;
const Colon = 0x3a;
const LessThanSign = 0x3c;
const EqualsSign = 0x3d;
const GreaterThanSign = 0x3e;
const QuestionMark = 0x3f;
const LatinCapitalLetterA = 0x41;
const LatinCapitalLetterB = 0x42;
const LatinCapitalLetterD = 0x44;
const LatinCapitalLetterF = 0x46;
const LatinCapitalLetterP = 0x50;
const LatinCapitalLetterS = 0x53;
const LatinCapitalLetterW = 0x57;
const LatinCapitalLetterZ = 0x5a;
const LowLine = 0x5f;
const LatinSmallLetterA = 0x61;
const LatinSmallLetterB = 0x62;
const LatinSmallLetterC = 0x63;
const LatinSmallLetterD = 0x64;
const LatinSmallLetterF = 0x66;
const LatinSmallLetterG = 0x67;
const LatinSmallLetterI = 0x69;
const LatinSmallLetterK = 0x6b;
const LatinSmallLetterM = 0x6d;
const LatinSmallLetterN = 0x6e;
const LatinSmallLetterP = 0x70;
const LatinSmallLetterR = 0x72;
const LatinSmallLetterS = 0x73;
const LatinSmallLetterT = 0x74;
const LatinSmallLetterU = 0x75;
const LatinSmallLetterV = 0x76;
const LatinSmallLetterW = 0x77;
const LatinSmallLetterX = 0x78;
const LatinSmallLetterY = 0x79;
const LatinSmallLetterZ = 0x7a;
const LeftSquareBracket = 0x5b;
const ReverseSolidus = 0x5c;
const RightSquareBracket = 0x5d;
const CircumflexAccent = 0x5e;
const LeftCurlyBracket = 0x7b;
const VerticalLine = 0x7c;
const RightCurlyBracket = 0x7d;
const ZeroWidthNonJoiner = 0x200c;
const ZeroWidthJoiner = 0x200d;
const LineSeparator = 0x2028;
const ParagraphSeparator = 0x2029;
const MinCodePoint = 0x00;
const MaxCodePoint = 0x10ffff;
function isLatinLetter(code) {
    return ((code >= LatinCapitalLetterA && code <= LatinCapitalLetterZ) ||
        (code >= LatinSmallLetterA && code <= LatinSmallLetterZ));
}
function isDecimalDigit(code) {
    return code >= DigitZero && code <= DigitNine;
}
function isOctalDigit(code) {
    return code >= DigitZero && code <= DigitSeven;
}
function isHexDigit(code) {
    return ((code >= DigitZero && code <= DigitNine) ||
        (code >= LatinCapitalLetterA && code <= LatinCapitalLetterF) ||
        (code >= LatinSmallLetterA && code <= LatinSmallLetterF));
}
function isLineTerminator(code) {
    return (code === LineFeed ||
        code === CarriageReturn ||
        code === LineSeparator ||
        code === ParagraphSeparator);
}
function isValidUnicode(code) {
    return code >= MinCodePoint && code <= MaxCodePoint;
}
function digitToInt(code) {
    if (code >= LatinSmallLetterA && code <= LatinSmallLetterF) {
        return code - LatinSmallLetterA + 10;
    }
    if (code >= LatinCapitalLetterA && code <= LatinCapitalLetterF) {
        return code - LatinCapitalLetterA + 10;
    }
    return code - DigitZero;
}
function isLeadSurrogate(code) {
    return code >= 0xd800 && code <= 0xdbff;
}
function isTrailSurrogate(code) {
    return code >= 0xdc00 && code <= 0xdfff;
}
function combineSurrogatePair(lead, trail) {
    return (lead - 0xd800) * 0x400 + (trail - 0xdc00) + 0x10000;
}

const legacyImpl = {
    at(s, end, i) {
        return i < end ? s.charCodeAt(i) : -1;
    },
    width(c) {
        return 1;
    },
};
const unicodeImpl = {
    at(s, end, i) {
        return i < end ? s.codePointAt(i) : -1;
    },
    width(c) {
        return c > 0xffff ? 2 : 1;
    },
};
class Reader {
    constructor() {
        this._impl = legacyImpl;
        this._s = "";
        this._i = 0;
        this._end = 0;
        this._cp1 = -1;
        this._w1 = 1;
        this._cp2 = -1;
        this._w2 = 1;
        this._cp3 = -1;
        this._w3 = 1;
        this._cp4 = -1;
    }
    get source() {
        return this._s;
    }
    get index() {
        return this._i;
    }
    get currentCodePoint() {
        return this._cp1;
    }
    get nextCodePoint() {
        return this._cp2;
    }
    get nextCodePoint2() {
        return this._cp3;
    }
    get nextCodePoint3() {
        return this._cp4;
    }
    reset(source, start, end, uFlag) {
        this._impl = uFlag ? unicodeImpl : legacyImpl;
        this._s = source;
        this._end = end;
        this.rewind(start);
    }
    rewind(index) {
        const impl = this._impl;
        this._i = index;
        this._cp1 = impl.at(this._s, this._end, index);
        this._w1 = impl.width(this._cp1);
        this._cp2 = impl.at(this._s, this._end, index + this._w1);
        this._w2 = impl.width(this._cp2);
        this._cp3 = impl.at(this._s, this._end, index + this._w1 + this._w2);
        this._w3 = impl.width(this._cp3);
        this._cp4 = impl.at(this._s, this._end, index + this._w1 + this._w2 + this._w3);
    }
    advance() {
        if (this._cp1 !== -1) {
            const impl = this._impl;
            this._i += this._w1;
            this._cp1 = this._cp2;
            this._w1 = this._w2;
            this._cp2 = this._cp3;
            this._w2 = impl.width(this._cp2);
            this._cp3 = this._cp4;
            this._w3 = impl.width(this._cp3);
            this._cp4 = impl.at(this._s, this._end, this._i + this._w1 + this._w2 + this._w3);
        }
    }
    eat(cp) {
        if (this._cp1 === cp) {
            this.advance();
            return true;
        }
        return false;
    }
    eat2(cp1, cp2) {
        if (this._cp1 === cp1 && this._cp2 === cp2) {
            this.advance();
            this.advance();
            return true;
        }
        return false;
    }
    eat3(cp1, cp2, cp3) {
        if (this._cp1 === cp1 && this._cp2 === cp2 && this._cp3 === cp3) {
            this.advance();
            this.advance();
            this.advance();
            return true;
        }
        return false;
    }
}

class RegExpSyntaxError extends SyntaxError {
    constructor(source, uFlag, index, message) {
        if (source) {
            if (!source.startsWith("/")) {
                source = `/${source}/${uFlag ? "u" : ""}`;
            }
            source = `: ${source}`;
        }
        super(`Invalid regular expression${source}: ${message}`);
        this.index = index;
    }
}

function isSyntaxCharacter(cp) {
    return (cp === CircumflexAccent ||
        cp === DollarSign ||
        cp === ReverseSolidus ||
        cp === FullStop ||
        cp === Asterisk ||
        cp === PlusSign ||
        cp === QuestionMark ||
        cp === LeftParenthesis ||
        cp === RightParenthesis ||
        cp === LeftSquareBracket ||
        cp === RightSquareBracket ||
        cp === LeftCurlyBracket ||
        cp === RightCurlyBracket ||
        cp === VerticalLine);
}
function isRegExpIdentifierStart(cp) {
    return isIdStart(cp) || cp === DollarSign || cp === LowLine;
}
function isRegExpIdentifierPart(cp) {
    return (isIdContinue(cp) ||
        cp === DollarSign ||
        cp === LowLine ||
        cp === ZeroWidthNonJoiner ||
        cp === ZeroWidthJoiner);
}
function isUnicodePropertyNameCharacter(cp) {
    return isLatinLetter(cp) || cp === LowLine;
}
function isUnicodePropertyValueCharacter(cp) {
    return isUnicodePropertyNameCharacter(cp) || isDecimalDigit(cp);
}
class RegExpValidator {
    constructor(options) {
        this._reader = new Reader();
        this._uFlag = false;
        this._nFlag = false;
        this._lastIntValue = 0;
        this._lastMinValue = 0;
        this._lastMaxValue = 0;
        this._lastStrValue = "";
        this._lastKeyValue = "";
        this._lastValValue = "";
        this._lastAssertionIsQuantifiable = false;
        this._numCapturingParens = 0;
        this._groupNames = new Set();
        this._backreferenceNames = new Set();
        this._options = options || {};
    }
    validateLiteral(source, start = 0, end = source.length) {
        this._uFlag = this._nFlag = false;
        this.reset(source, start, end);
        this.onLiteralEnter(start);
        if (this.eat(Solidus) && this.eatRegExpBody() && this.eat(Solidus)) {
            const flagStart = this.index;
            const uFlag = source.includes("u", flagStart);
            this.validateFlags(source, flagStart, end);
            this.validatePattern(source, start + 1, flagStart - 1, uFlag);
        }
        else if (start >= end) {
            this.raise("Empty");
        }
        else {
            const c = String.fromCodePoint(this.currentCodePoint);
            this.raise(`Unexpected character '${c}'`);
        }
        this.onLiteralLeave(start, end);
    }
    validateFlags(source, start = 0, end = source.length) {
        const existingFlags = new Set();
        let global = false;
        let ignoreCase = false;
        let multiline = false;
        let sticky = false;
        let unicode = false;
        let dotAll = false;
        for (let i = start; i < end; ++i) {
            const flag = source.charCodeAt(i);
            if (existingFlags.has(flag)) {
                this.raise(`Duplicated flag '${source[i]}'`);
            }
            existingFlags.add(flag);
            if (flag === LatinSmallLetterG) {
                global = true;
            }
            else if (flag === LatinSmallLetterI) {
                ignoreCase = true;
            }
            else if (flag === LatinSmallLetterM) {
                multiline = true;
            }
            else if (flag === LatinSmallLetterU && this.ecmaVersion >= 2015) {
                unicode = true;
            }
            else if (flag === LatinSmallLetterY && this.ecmaVersion >= 2015) {
                sticky = true;
            }
            else if (flag === LatinSmallLetterS && this.ecmaVersion >= 2018) {
                dotAll = true;
            }
            else {
                this.raise(`Invalid flag '${source[i]}'`);
            }
        }
        this.onFlags(start, end, global, ignoreCase, multiline, unicode, sticky, dotAll);
    }
    validatePattern(source, start = 0, end = source.length, uFlag = false) {
        this._uFlag = uFlag && this.ecmaVersion >= 2015;
        this._nFlag = uFlag && this.ecmaVersion >= 2018;
        this.reset(source, start, end);
        this.consumePattern();
        if (!this._nFlag &&
            this.ecmaVersion >= 2018 &&
            this._groupNames.size > 0) {
            this._nFlag = true;
            this.rewind(start);
            this.consumePattern();
        }
    }
    get strict() {
        return Boolean(this._options.strict || this._uFlag);
    }
    get ecmaVersion() {
        return this._options.ecmaVersion || 2020;
    }
    onLiteralEnter(start) {
        if (this._options.onLiteralEnter) {
            this._options.onLiteralEnter(start);
        }
    }
    onLiteralLeave(start, end) {
        if (this._options.onLiteralLeave) {
            this._options.onLiteralLeave(start, end);
        }
    }
    onFlags(start, end, global, ignoreCase, multiline, unicode, sticky, dotAll) {
        if (this._options.onFlags) {
            this._options.onFlags(start, end, global, ignoreCase, multiline, unicode, sticky, dotAll);
        }
    }
    onPatternEnter(start) {
        if (this._options.onPatternEnter) {
            this._options.onPatternEnter(start);
        }
    }
    onPatternLeave(start, end) {
        if (this._options.onPatternLeave) {
            this._options.onPatternLeave(start, end);
        }
    }
    onDisjunctionEnter(start) {
        if (this._options.onDisjunctionEnter) {
            this._options.onDisjunctionEnter(start);
        }
    }
    onDisjunctionLeave(start, end) {
        if (this._options.onDisjunctionLeave) {
            this._options.onDisjunctionLeave(start, end);
        }
    }
    onAlternativeEnter(start, index) {
        if (this._options.onAlternativeEnter) {
            this._options.onAlternativeEnter(start, index);
        }
    }
    onAlternativeLeave(start, end, index) {
        if (this._options.onAlternativeLeave) {
            this._options.onAlternativeLeave(start, end, index);
        }
    }
    onGroupEnter(start) {
        if (this._options.onGroupEnter) {
            this._options.onGroupEnter(start);
        }
    }
    onGroupLeave(start, end) {
        if (this._options.onGroupLeave) {
            this._options.onGroupLeave(start, end);
        }
    }
    onCapturingGroupEnter(start, name) {
        if (this._options.onCapturingGroupEnter) {
            this._options.onCapturingGroupEnter(start, name);
        }
    }
    onCapturingGroupLeave(start, end, name) {
        if (this._options.onCapturingGroupLeave) {
            this._options.onCapturingGroupLeave(start, end, name);
        }
    }
    onQuantifier(start, end, min, max, greedy) {
        if (this._options.onQuantifier) {
            this._options.onQuantifier(start, end, min, max, greedy);
        }
    }
    onLookaroundAssertionEnter(start, kind, negate) {
        if (this._options.onLookaroundAssertionEnter) {
            this._options.onLookaroundAssertionEnter(start, kind, negate);
        }
    }
    onLookaroundAssertionLeave(start, end, kind, negate) {
        if (this._options.onLookaroundAssertionLeave) {
            this._options.onLookaroundAssertionLeave(start, end, kind, negate);
        }
    }
    onEdgeAssertion(start, end, kind) {
        if (this._options.onEdgeAssertion) {
            this._options.onEdgeAssertion(start, end, kind);
        }
    }
    onWordBoundaryAssertion(start, end, kind, negate) {
        if (this._options.onWordBoundaryAssertion) {
            this._options.onWordBoundaryAssertion(start, end, kind, negate);
        }
    }
    onAnyCharacterSet(start, end, kind) {
        if (this._options.onAnyCharacterSet) {
            this._options.onAnyCharacterSet(start, end, kind);
        }
    }
    onEscapeCharacterSet(start, end, kind, negate) {
        if (this._options.onEscapeCharacterSet) {
            this._options.onEscapeCharacterSet(start, end, kind, negate);
        }
    }
    onUnicodePropertyCharacterSet(start, end, kind, key, value, negate) {
        if (this._options.onUnicodePropertyCharacterSet) {
            this._options.onUnicodePropertyCharacterSet(start, end, kind, key, value, negate);
        }
    }
    onCharacter(start, end, value) {
        if (this._options.onCharacter) {
            this._options.onCharacter(start, end, value);
        }
    }
    onBackreference(start, end, ref) {
        if (this._options.onBackreference) {
            this._options.onBackreference(start, end, ref);
        }
    }
    onCharacterClassEnter(start, negate) {
        if (this._options.onCharacterClassEnter) {
            this._options.onCharacterClassEnter(start, negate);
        }
    }
    onCharacterClassLeave(start, end, negate) {
        if (this._options.onCharacterClassLeave) {
            this._options.onCharacterClassLeave(start, end, negate);
        }
    }
    onCharacterClassRange(start, end, min, max) {
        if (this._options.onCharacterClassRange) {
            this._options.onCharacterClassRange(start, end, min, max);
        }
    }
    get source() {
        return this._reader.source;
    }
    get index() {
        return this._reader.index;
    }
    get currentCodePoint() {
        return this._reader.currentCodePoint;
    }
    get nextCodePoint() {
        return this._reader.nextCodePoint;
    }
    get nextCodePoint2() {
        return this._reader.nextCodePoint2;
    }
    get nextCodePoint3() {
        return this._reader.nextCodePoint3;
    }
    reset(source, start, end) {
        this._reader.reset(source, start, end, this._uFlag);
    }
    rewind(index) {
        this._reader.rewind(index);
    }
    advance() {
        this._reader.advance();
    }
    eat(cp) {
        return this._reader.eat(cp);
    }
    eat2(cp1, cp2) {
        return this._reader.eat2(cp1, cp2);
    }
    eat3(cp1, cp2, cp3) {
        return this._reader.eat3(cp1, cp2, cp3);
    }
    raise(message) {
        throw new RegExpSyntaxError(this.source, this._uFlag, this.index, message);
    }
    eatRegExpBody() {
        const start = this.index;
        let inClass = false;
        let escaped = false;
        for (;;) {
            const cp = this.currentCodePoint;
            if (cp === -1 || isLineTerminator(cp)) {
                const kind = inClass ? "character class" : "regular expression";
                this.raise(`Unterminated ${kind}`);
            }
            if (escaped) {
                escaped = false;
            }
            else if (cp === ReverseSolidus) {
                escaped = true;
            }
            else if (cp === LeftSquareBracket) {
                inClass = true;
            }
            else if (cp === RightSquareBracket) {
                inClass = false;
            }
            else if ((cp === Solidus && !inClass) ||
                (cp === Asterisk && this.index === start)) {
                break;
            }
            this.advance();
        }
        return this.index !== start;
    }
    consumePattern() {
        const start = this.index;
        this._numCapturingParens = this.countCapturingParens();
        this._groupNames.clear();
        this._backreferenceNames.clear();
        this.onPatternEnter(start);
        this.consumeDisjunction();
        const cp = this.currentCodePoint;
        if (this.currentCodePoint !== -1) {
            if (cp === RightParenthesis) {
                this.raise("Unmatched ')'");
            }
            if (cp === ReverseSolidus) {
                this.raise("\\ at end of pattern");
            }
            if (cp === RightSquareBracket || cp === RightCurlyBracket) {
                this.raise("Lone quantifier brackets");
            }
            const c = String.fromCodePoint(cp);
            this.raise(`Unexpected character '${c}'`);
        }
        for (const name of this._backreferenceNames) {
            if (!this._groupNames.has(name)) {
                this.raise("Invalid named capture referenced");
            }
        }
        this.onPatternLeave(start, this.index);
    }
    countCapturingParens() {
        const start = this.index;
        let inClass = false;
        let escaped = false;
        let count = 0;
        let cp = 0;
        while ((cp = this.currentCodePoint) !== -1) {
            if (escaped) {
                escaped = false;
            }
            else if (cp === ReverseSolidus) {
                escaped = true;
            }
            else if (cp === LeftSquareBracket) {
                inClass = true;
            }
            else if (cp === RightSquareBracket) {
                inClass = false;
            }
            else if (cp === LeftParenthesis &&
                !inClass &&
                (this.nextCodePoint !== QuestionMark ||
                    (this.nextCodePoint2 === LessThanSign &&
                        this.nextCodePoint3 !== EqualsSign &&
                        this.nextCodePoint3 !== ExclamationMark))) {
                count += 1;
            }
            this.advance();
        }
        this.rewind(start);
        return count;
    }
    consumeDisjunction() {
        const start = this.index;
        let i = 0;
        this.onDisjunctionEnter(start);
        do {
            this.consumeAlternative(i++);
        } while (this.eat(VerticalLine));
        if (this.consumeQuantifier(true)) {
            this.raise("Nothing to repeat");
        }
        if (this.eat(LeftCurlyBracket)) {
            this.raise("Lone quantifier brackets");
        }
        this.onDisjunctionLeave(start, this.index);
    }
    consumeAlternative(i) {
        const start = this.index;
        this.onAlternativeEnter(start, i);
        while (this.currentCodePoint !== -1 && this.consumeTerm()) {
        }
        this.onAlternativeLeave(start, this.index, i);
    }
    consumeTerm() {
        if (this._uFlag || this.strict) {
            return (this.consumeAssertion() ||
                (this.consumeAtom() && this.consumeOptionalQuantifier()));
        }
        return ((this.consumeAssertion() &&
            (!this._lastAssertionIsQuantifiable ||
                this.consumeOptionalQuantifier())) ||
            (this.consumeExtendedAtom() && this.consumeOptionalQuantifier()));
    }
    consumeOptionalQuantifier() {
        this.consumeQuantifier();
        return true;
    }
    consumeAssertion() {
        const start = this.index;
        this._lastAssertionIsQuantifiable = false;
        if (this.eat(CircumflexAccent)) {
            this.onEdgeAssertion(start, this.index, "start");
            return true;
        }
        if (this.eat(DollarSign)) {
            this.onEdgeAssertion(start, this.index, "end");
            return true;
        }
        if (this.eat2(ReverseSolidus, LatinCapitalLetterB)) {
            this.onWordBoundaryAssertion(start, this.index, "word", true);
            return true;
        }
        if (this.eat2(ReverseSolidus, LatinSmallLetterB)) {
            this.onWordBoundaryAssertion(start, this.index, "word", false);
            return true;
        }
        if (this.eat2(LeftParenthesis, QuestionMark)) {
            const lookbehind = this.ecmaVersion >= 2018 && this.eat(LessThanSign);
            let negate = false;
            if (this.eat(EqualsSign) || (negate = this.eat(ExclamationMark))) {
                const kind = lookbehind ? "lookbehind" : "lookahead";
                this.onLookaroundAssertionEnter(start, kind, negate);
                this.consumeDisjunction();
                if (!this.eat(RightParenthesis)) {
                    this.raise("Unterminated group");
                }
                this._lastAssertionIsQuantifiable = !lookbehind && !this.strict;
                this.onLookaroundAssertionLeave(start, this.index, kind, negate);
                return true;
            }
            this.rewind(start);
        }
        return false;
    }
    consumeQuantifier(noConsume = false) {
        const start = this.index;
        let min = 0;
        let max = 0;
        let greedy = false;
        if (this.eat(Asterisk)) {
            min = 0;
            max = Number.POSITIVE_INFINITY;
        }
        else if (this.eat(PlusSign)) {
            min = 1;
            max = Number.POSITIVE_INFINITY;
        }
        else if (this.eat(QuestionMark)) {
            min = 0;
            max = 1;
        }
        else if (this.eatBracedQuantifier(noConsume)) {
            min = this._lastMinValue;
            max = this._lastMaxValue;
        }
        else {
            return false;
        }
        greedy = !this.eat(QuestionMark);
        if (!noConsume) {
            this.onQuantifier(start, this.index, min, max, greedy);
        }
        return true;
    }
    eatBracedQuantifier(noError) {
        const start = this.index;
        if (this.eat(LeftCurlyBracket)) {
            this._lastMinValue = 0;
            this._lastMaxValue = Number.POSITIVE_INFINITY;
            if (this.eatDecimalDigits()) {
                this._lastMinValue = this._lastMaxValue = this._lastIntValue;
                if (this.eat(Comma)) {
                    this._lastMaxValue = this.eatDecimalDigits()
                        ? this._lastIntValue
                        : Number.POSITIVE_INFINITY;
                }
                if (this.eat(RightCurlyBracket)) {
                    if (!noError && this._lastMaxValue < this._lastMinValue) {
                        this.raise("numbers out of order in {} quantifier");
                    }
                    return true;
                }
            }
            if (!noError && (this._uFlag || this.strict)) {
                this.raise("Incomplete quantifier");
            }
            this.rewind(start);
        }
        return false;
    }
    consumeAtom() {
        return (this.consumePatternCharacter() ||
            this.consumeDot() ||
            this.consumeReverseSolidusAtomEscape() ||
            this.consumeCharacterClass() ||
            this.consumeUncapturingGroup() ||
            this.consumeCapturingGroup());
    }
    consumeDot() {
        if (this.eat(FullStop)) {
            this.onAnyCharacterSet(this.index - 1, this.index, "any");
            return true;
        }
        return false;
    }
    consumeReverseSolidusAtomEscape() {
        const start = this.index;
        if (this.eat(ReverseSolidus)) {
            if (this.consumeAtomEscape()) {
                return true;
            }
            this.rewind(start);
        }
        return false;
    }
    consumeUncapturingGroup() {
        const start = this.index;
        if (this.eat3(LeftParenthesis, QuestionMark, Colon)) {
            this.onGroupEnter(start);
            this.consumeDisjunction();
            if (!this.eat(RightParenthesis)) {
                this.raise("Unterminated group");
            }
            this.onGroupLeave(start, this.index);
            return true;
        }
        return false;
    }
    consumeCapturingGroup() {
        const start = this.index;
        if (this.eat(LeftParenthesis)) {
            let name = null;
            if (this.ecmaVersion >= 2018) {
                if (this.consumeGroupSpecifier()) {
                    name = this._lastStrValue;
                }
            }
            else if (this.currentCodePoint === QuestionMark) {
                this.raise("Invalid group");
            }
            this.onCapturingGroupEnter(start, name);
            this.consumeDisjunction();
            if (!this.eat(RightParenthesis)) {
                this.raise("Unterminated group");
            }
            this.onCapturingGroupLeave(start, this.index, name);
            return true;
        }
        return false;
    }
    consumeExtendedAtom() {
        return (this.consumeDot() ||
            this.consumeReverseSolidusAtomEscape() ||
            this.consumeReverseSolidusFollowedByC() ||
            this.consumeCharacterClass() ||
            this.consumeUncapturingGroup() ||
            this.consumeCapturingGroup() ||
            this.consumeInvalidBracedQuantifier() ||
            this.consumeExtendedPatternCharacter());
    }
    consumeReverseSolidusFollowedByC() {
        const start = this.index;
        if (this.currentCodePoint === ReverseSolidus &&
            this.nextCodePoint === LatinSmallLetterC) {
            this._lastIntValue = this.currentCodePoint;
            this.advance();
            this.onCharacter(start, this.index, ReverseSolidus);
            return true;
        }
        return false;
    }
    consumeInvalidBracedQuantifier() {
        if (this.eatBracedQuantifier(true)) {
            this.raise("Nothing to repeat");
        }
        return false;
    }
    consumePatternCharacter() {
        const start = this.index;
        const cp = this.currentCodePoint;
        if (cp !== -1 && !isSyntaxCharacter(cp)) {
            this.advance();
            this.onCharacter(start, this.index, cp);
            return true;
        }
        return false;
    }
    consumeExtendedPatternCharacter() {
        const start = this.index;
        const cp = this.currentCodePoint;
        if (cp !== -1 &&
            cp !== CircumflexAccent &&
            cp !== DollarSign &&
            cp !== ReverseSolidus &&
            cp !== FullStop &&
            cp !== Asterisk &&
            cp !== PlusSign &&
            cp !== QuestionMark &&
            cp !== LeftParenthesis &&
            cp !== RightParenthesis &&
            cp !== LeftSquareBracket &&
            cp !== VerticalLine) {
            this.advance();
            this.onCharacter(start, this.index, cp);
            return true;
        }
        return false;
    }
    consumeGroupSpecifier() {
        if (this.eat(QuestionMark)) {
            if (this.eatGroupName()) {
                if (!this._groupNames.has(this._lastStrValue)) {
                    this._groupNames.add(this._lastStrValue);
                    return true;
                }
                this.raise("Duplicate capture group name");
            }
            this.raise("Invalid group");
        }
        return false;
    }
    consumeAtomEscape() {
        if (this.consumeBackreference() ||
            this.consumeCharacterClassEscape() ||
            this.consumeCharacterEscape() ||
            (this._nFlag && this.consumeKGroupName())) {
            return true;
        }
        if (this.strict || this._uFlag) {
            this.raise("Invalid escape");
        }
        return false;
    }
    consumeBackreference() {
        const start = this.index;
        if (this.eatDecimalEscape()) {
            const n = this._lastIntValue;
            if (n <= this._numCapturingParens) {
                this.onBackreference(start - 1, this.index, n);
                return true;
            }
            if (this.strict || this._uFlag) {
                this.raise("Invalid escape");
            }
            this.rewind(start);
        }
        return false;
    }
    consumeCharacterClassEscape() {
        const start = this.index;
        if (this.eat(LatinSmallLetterD)) {
            this._lastIntValue = -1;
            this.onEscapeCharacterSet(start - 1, this.index, "digit", false);
            return true;
        }
        if (this.eat(LatinCapitalLetterD)) {
            this._lastIntValue = -1;
            this.onEscapeCharacterSet(start - 1, this.index, "digit", true);
            return true;
        }
        if (this.eat(LatinSmallLetterS)) {
            this._lastIntValue = -1;
            this.onEscapeCharacterSet(start - 1, this.index, "space", false);
            return true;
        }
        if (this.eat(LatinCapitalLetterS)) {
            this._lastIntValue = -1;
            this.onEscapeCharacterSet(start - 1, this.index, "space", true);
            return true;
        }
        if (this.eat(LatinSmallLetterW)) {
            this._lastIntValue = -1;
            this.onEscapeCharacterSet(start - 1, this.index, "word", false);
            return true;
        }
        if (this.eat(LatinCapitalLetterW)) {
            this._lastIntValue = -1;
            this.onEscapeCharacterSet(start - 1, this.index, "word", true);
            return true;
        }
        let negate = false;
        if (this._uFlag &&
            this.ecmaVersion >= 2018 &&
            (this.eat(LatinSmallLetterP) ||
                (negate = this.eat(LatinCapitalLetterP)))) {
            this._lastIntValue = -1;
            if (this.eat(LeftCurlyBracket) &&
                this.eatUnicodePropertyValueExpression() &&
                this.eat(RightCurlyBracket)) {
                this.onUnicodePropertyCharacterSet(start - 1, this.index, "property", this._lastKeyValue, this._lastValValue || null, negate);
                return true;
            }
            this.raise("Invalid property name");
        }
        return false;
    }
    consumeCharacterEscape() {
        const start = this.index;
        if (this.eatControlEscape() ||
            this.eatCControlLetter() ||
            this.eatZero() ||
            this.eatHexEscapeSequence() ||
            this.eatRegExpUnicodeEscapeSequence() ||
            (!this.strict &&
                !this._uFlag &&
                this.eatLegacyOctalEscapeSequence()) ||
            this.eatIdentityEscape()) {
            this.onCharacter(start - 1, this.index, this._lastIntValue);
            return true;
        }
        return false;
    }
    consumeKGroupName() {
        const start = this.index;
        if (this.eat(LatinSmallLetterK)) {
            if (this.eatGroupName()) {
                const groupName = this._lastStrValue;
                this._backreferenceNames.add(groupName);
                this.onBackreference(start - 1, this.index, groupName);
                return true;
            }
            this.raise("Invalid named reference");
        }
        return false;
    }
    consumeCharacterClass() {
        const start = this.index;
        if (this.eat(LeftSquareBracket)) {
            const negate = this.eat(CircumflexAccent);
            this.onCharacterClassEnter(start, negate);
            this.consumeClassRanges();
            if (!this.eat(RightSquareBracket)) {
                this.raise("Unterminated character class");
            }
            this.onCharacterClassLeave(start, this.index, negate);
            return true;
        }
        return false;
    }
    consumeClassRanges() {
        const strict = this.strict || this._uFlag;
        for (;;) {
            const rangeStart = this.index;
            if (!this.consumeClassAtom()) {
                break;
            }
            const min = this._lastIntValue;
            if (!this.eat(HyphenMinus)) {
                continue;
            }
            this.onCharacter(this.index - 1, this.index, HyphenMinus);
            if (!this.consumeClassAtom()) {
                break;
            }
            const max = this._lastIntValue;
            if (min === -1 || max === -1) {
                if (strict) {
                    this.raise("Invalid character class");
                }
                continue;
            }
            if (min > max) {
                this.raise("Range out of order in character class");
            }
            this.onCharacterClassRange(rangeStart, this.index, min, max);
        }
    }
    consumeClassAtom() {
        const start = this.index;
        const cp = this.currentCodePoint;
        if (cp !== -1 && cp !== ReverseSolidus && cp !== RightSquareBracket) {
            this.advance();
            this._lastIntValue = cp;
            this.onCharacter(start, this.index, this._lastIntValue);
            return true;
        }
        if (this.eat(ReverseSolidus)) {
            if (this.consumeClassEscape()) {
                return true;
            }
            if (!this.strict && this.currentCodePoint === LatinSmallLetterC) {
                this._lastIntValue = ReverseSolidus;
                this.onCharacter(start, this.index, this._lastIntValue);
                return true;
            }
            if (this.strict || this._uFlag) {
                this.raise("Invalid escape");
            }
            this.rewind(start);
        }
        return false;
    }
    consumeClassEscape() {
        const start = this.index;
        if (this.eat(LatinSmallLetterB)) {
            this._lastIntValue = Backspace;
            this.onCharacter(start - 1, this.index, this._lastIntValue);
            return true;
        }
        if (this._uFlag && this.eat(HyphenMinus)) {
            this._lastIntValue = HyphenMinus;
            this.onCharacter(start - 1, this.index, this._lastIntValue);
            return true;
        }
        let cp = 0;
        if (!this.strict &&
            !this._uFlag &&
            this.currentCodePoint === LatinSmallLetterC &&
            (isDecimalDigit((cp = this.nextCodePoint)) || cp === LowLine)) {
            this.advance();
            this.advance();
            this._lastIntValue = cp % 0x20;
            this.onCharacter(start - 1, this.index, this._lastIntValue);
            return true;
        }
        return (this.consumeCharacterClassEscape() || this.consumeCharacterEscape());
    }
    eatGroupName() {
        if (this.eat(LessThanSign)) {
            if (this.eatRegExpIdentifierName() && this.eat(GreaterThanSign)) {
                return true;
            }
            this.raise("Invalid capture group name");
        }
        return false;
    }
    eatRegExpIdentifierName() {
        if (this.eatRegExpIdentifierStart()) {
            this._lastStrValue = String.fromCodePoint(this._lastIntValue);
            while (this.eatRegExpIdentifierPart()) {
                this._lastStrValue += String.fromCodePoint(this._lastIntValue);
            }
            return true;
        }
        return false;
    }
    eatRegExpIdentifierStart() {
        const start = this.index;
        const forceUFlag = !this._uFlag && this.ecmaVersion >= 2020;
        let cp = this.currentCodePoint;
        this.advance();
        if (cp === ReverseSolidus &&
            this.eatRegExpUnicodeEscapeSequence(forceUFlag)) {
            cp = this._lastIntValue;
        }
        else if (forceUFlag &&
            isLeadSurrogate(cp) &&
            isTrailSurrogate(this.currentCodePoint)) {
            cp = combineSurrogatePair(cp, this.currentCodePoint);
            this.advance();
        }
        if (isRegExpIdentifierStart(cp)) {
            this._lastIntValue = cp;
            return true;
        }
        if (this.index !== start) {
            this.rewind(start);
        }
        return false;
    }
    eatRegExpIdentifierPart() {
        const start = this.index;
        const forceUFlag = !this._uFlag && this.ecmaVersion >= 2020;
        let cp = this.currentCodePoint;
        this.advance();
        if (cp === ReverseSolidus &&
            this.eatRegExpUnicodeEscapeSequence(forceUFlag)) {
            cp = this._lastIntValue;
        }
        else if (forceUFlag &&
            isLeadSurrogate(cp) &&
            isTrailSurrogate(this.currentCodePoint)) {
            cp = combineSurrogatePair(cp, this.currentCodePoint);
            this.advance();
        }
        if (isRegExpIdentifierPart(cp)) {
            this._lastIntValue = cp;
            return true;
        }
        if (this.index !== start) {
            this.rewind(start);
        }
        return false;
    }
    eatCControlLetter() {
        const start = this.index;
        if (this.eat(LatinSmallLetterC)) {
            if (this.eatControlLetter()) {
                return true;
            }
            this.rewind(start);
        }
        return false;
    }
    eatZero() {
        if (this.currentCodePoint === DigitZero &&
            !isDecimalDigit(this.nextCodePoint)) {
            this._lastIntValue = 0;
            this.advance();
            return true;
        }
        return false;
    }
    eatControlEscape() {
        if (this.eat(LatinSmallLetterF)) {
            this._lastIntValue = FormFeed;
            return true;
        }
        if (this.eat(LatinSmallLetterN)) {
            this._lastIntValue = LineFeed;
            return true;
        }
        if (this.eat(LatinSmallLetterR)) {
            this._lastIntValue = CarriageReturn;
            return true;
        }
        if (this.eat(LatinSmallLetterT)) {
            this._lastIntValue = CharacterTabulation;
            return true;
        }
        if (this.eat(LatinSmallLetterV)) {
            this._lastIntValue = LineTabulation;
            return true;
        }
        return false;
    }
    eatControlLetter() {
        const cp = this.currentCodePoint;
        if (isLatinLetter(cp)) {
            this.advance();
            this._lastIntValue = cp % 0x20;
            return true;
        }
        return false;
    }
    eatRegExpUnicodeEscapeSequence(forceUFlag = false) {
        const start = this.index;
        const uFlag = forceUFlag || this._uFlag;
        if (this.eat(LatinSmallLetterU)) {
            if ((uFlag && this.eatRegExpUnicodeSurrogatePairEscape()) ||
                this.eatFixedHexDigits(4) ||
                (uFlag && this.eatRegExpUnicodeCodePointEscape())) {
                return true;
            }
            if (this.strict || uFlag) {
                this.raise("Invalid unicode escape");
            }
            this.rewind(start);
        }
        return false;
    }
    eatRegExpUnicodeSurrogatePairEscape() {
        const start = this.index;
        if (this.eatFixedHexDigits(4)) {
            const lead = this._lastIntValue;
            if (isLeadSurrogate(lead) &&
                this.eat(ReverseSolidus) &&
                this.eat(LatinSmallLetterU) &&
                this.eatFixedHexDigits(4)) {
                const trail = this._lastIntValue;
                if (isTrailSurrogate(trail)) {
                    this._lastIntValue = combineSurrogatePair(lead, trail);
                    return true;
                }
            }
            this.rewind(start);
        }
        return false;
    }
    eatRegExpUnicodeCodePointEscape() {
        const start = this.index;
        if (this.eat(LeftCurlyBracket) &&
            this.eatHexDigits() &&
            this.eat(RightCurlyBracket) &&
            isValidUnicode(this._lastIntValue)) {
            return true;
        }
        this.rewind(start);
        return false;
    }
    eatIdentityEscape() {
        const cp = this.currentCodePoint;
        if (this.isValidIdentityEscape(cp)) {
            this._lastIntValue = cp;
            this.advance();
            return true;
        }
        return false;
    }
    isValidIdentityEscape(cp) {
        if (cp === -1) {
            return false;
        }
        if (this._uFlag) {
            return isSyntaxCharacter(cp) || cp === Solidus;
        }
        if (this.strict) {
            return !isIdContinue(cp);
        }
        if (this._nFlag) {
            return !(cp === LatinSmallLetterC || cp === LatinSmallLetterK);
        }
        return cp !== LatinSmallLetterC;
    }
    eatDecimalEscape() {
        this._lastIntValue = 0;
        let cp = this.currentCodePoint;
        if (cp >= DigitOne && cp <= DigitNine) {
            do {
                this._lastIntValue = 10 * this._lastIntValue + (cp - DigitZero);
                this.advance();
            } while ((cp = this.currentCodePoint) >= DigitZero &&
                cp <= DigitNine);
            return true;
        }
        return false;
    }
    eatUnicodePropertyValueExpression() {
        const start = this.index;
        if (this.eatUnicodePropertyName() && this.eat(EqualsSign)) {
            this._lastKeyValue = this._lastStrValue;
            if (this.eatUnicodePropertyValue()) {
                this._lastValValue = this._lastStrValue;
                if (isValidUnicodeProperty(this.ecmaVersion, this._lastKeyValue, this._lastValValue)) {
                    return true;
                }
                this.raise("Invalid property name");
            }
        }
        this.rewind(start);
        if (this.eatLoneUnicodePropertyNameOrValue()) {
            const nameOrValue = this._lastStrValue;
            if (isValidUnicodeProperty(this.ecmaVersion, "General_Category", nameOrValue)) {
                this._lastKeyValue = "General_Category";
                this._lastValValue = nameOrValue;
                return true;
            }
            if (isValidLoneUnicodeProperty(this.ecmaVersion, nameOrValue)) {
                this._lastKeyValue = nameOrValue;
                this._lastValValue = "";
                return true;
            }
            this.raise("Invalid property name");
        }
        return false;
    }
    eatUnicodePropertyName() {
        this._lastStrValue = "";
        while (isUnicodePropertyNameCharacter(this.currentCodePoint)) {
            this._lastStrValue += String.fromCodePoint(this.currentCodePoint);
            this.advance();
        }
        return this._lastStrValue !== "";
    }
    eatUnicodePropertyValue() {
        this._lastStrValue = "";
        while (isUnicodePropertyValueCharacter(this.currentCodePoint)) {
            this._lastStrValue += String.fromCodePoint(this.currentCodePoint);
            this.advance();
        }
        return this._lastStrValue !== "";
    }
    eatLoneUnicodePropertyNameOrValue() {
        return this.eatUnicodePropertyValue();
    }
    eatHexEscapeSequence() {
        const start = this.index;
        if (this.eat(LatinSmallLetterX)) {
            if (this.eatFixedHexDigits(2)) {
                return true;
            }
            if (this._uFlag || this.strict) {
                this.raise("Invalid escape");
            }
            this.rewind(start);
        }
        return false;
    }
    eatDecimalDigits() {
        const start = this.index;
        this._lastIntValue = 0;
        while (isDecimalDigit(this.currentCodePoint)) {
            this._lastIntValue =
                10 * this._lastIntValue + digitToInt(this.currentCodePoint);
            this.advance();
        }
        return this.index !== start;
    }
    eatHexDigits() {
        const start = this.index;
        this._lastIntValue = 0;
        while (isHexDigit(this.currentCodePoint)) {
            this._lastIntValue =
                16 * this._lastIntValue + digitToInt(this.currentCodePoint);
            this.advance();
        }
        return this.index !== start;
    }
    eatLegacyOctalEscapeSequence() {
        if (this.eatOctalDigit()) {
            const n1 = this._lastIntValue;
            if (this.eatOctalDigit()) {
                const n2 = this._lastIntValue;
                if (n1 <= 3 && this.eatOctalDigit()) {
                    this._lastIntValue = n1 * 64 + n2 * 8 + this._lastIntValue;
                }
                else {
                    this._lastIntValue = n1 * 8 + n2;
                }
            }
            else {
                this._lastIntValue = n1;
            }
            return true;
        }
        return false;
    }
    eatOctalDigit() {
        const cp = this.currentCodePoint;
        if (isOctalDigit(cp)) {
            this.advance();
            this._lastIntValue = cp - DigitZero;
            return true;
        }
        this._lastIntValue = 0;
        return false;
    }
    eatFixedHexDigits(length) {
        const start = this.index;
        this._lastIntValue = 0;
        for (let i = 0; i < length; ++i) {
            const cp = this.currentCodePoint;
            if (!isHexDigit(cp)) {
                this.rewind(start);
                return false;
            }
            this._lastIntValue = 16 * this._lastIntValue + digitToInt(cp);
            this.advance();
        }
        return true;
    }
}

const DummyPattern = {};
const DummyFlags = {};
const DummyCapturingGroup = {};
class RegExpParserState {
    constructor(options) {
        this._node = DummyPattern;
        this._flags = DummyFlags;
        this._backreferences = [];
        this._capturingGroups = [];
        this.source = "";
        this.strict = Boolean(options && options.strict);
        this.ecmaVersion = (options && options.ecmaVersion) || 2020;
    }
    get pattern() {
        if (this._node.type !== "Pattern") {
            throw new Error("UnknownError");
        }
        return this._node;
    }
    get flags() {
        if (this._flags.type !== "Flags") {
            throw new Error("UnknownError");
        }
        return this._flags;
    }
    onFlags(start, end, global, ignoreCase, multiline, unicode, sticky, dotAll) {
        this._flags = {
            type: "Flags",
            parent: null,
            start,
            end,
            raw: this.source.slice(start, end),
            global,
            ignoreCase,
            multiline,
            unicode,
            sticky,
            dotAll,
        };
    }
    onPatternEnter(start) {
        this._node = {
            type: "Pattern",
            parent: null,
            start,
            end: start,
            raw: "",
            alternatives: [],
        };
        this._backreferences.length = 0;
        this._capturingGroups.length = 0;
    }
    onPatternLeave(start, end) {
        this._node.end = end;
        this._node.raw = this.source.slice(start, end);
        for (const reference of this._backreferences) {
            const ref = reference.ref;
            const group = typeof ref === "number"
                ? this._capturingGroups[ref - 1]
                : this._capturingGroups.find(g => g.name === ref);
            reference.resolved = group;
            group.references.push(reference);
        }
    }
    onAlternativeEnter(start) {
        const parent = this._node;
        if (parent.type !== "Assertion" &&
            parent.type !== "CapturingGroup" &&
            parent.type !== "Group" &&
            parent.type !== "Pattern") {
            throw new Error("UnknownError");
        }
        this._node = {
            type: "Alternative",
            parent,
            start,
            end: start,
            raw: "",
            elements: [],
        };
        parent.alternatives.push(this._node);
    }
    onAlternativeLeave(start, end) {
        const node = this._node;
        if (node.type !== "Alternative") {
            throw new Error("UnknownError");
        }
        node.end = end;
        node.raw = this.source.slice(start, end);
        this._node = node.parent;
    }
    onGroupEnter(start) {
        const parent = this._node;
        if (parent.type !== "Alternative") {
            throw new Error("UnknownError");
        }
        this._node = {
            type: "Group",
            parent,
            start,
            end: start,
            raw: "",
            alternatives: [],
        };
        parent.elements.push(this._node);
    }
    onGroupLeave(start, end) {
        const node = this._node;
        if (node.type !== "Group" || node.parent.type !== "Alternative") {
            throw new Error("UnknownError");
        }
        node.end = end;
        node.raw = this.source.slice(start, end);
        this._node = node.parent;
    }
    onCapturingGroupEnter(start, name) {
        const parent = this._node;
        if (parent.type !== "Alternative") {
            throw new Error("UnknownError");
        }
        this._node = {
            type: "CapturingGroup",
            parent,
            start,
            end: start,
            raw: "",
            name,
            alternatives: [],
            references: [],
        };
        parent.elements.push(this._node);
        this._capturingGroups.push(this._node);
    }
    onCapturingGroupLeave(start, end) {
        const node = this._node;
        if (node.type !== "CapturingGroup" ||
            node.parent.type !== "Alternative") {
            throw new Error("UnknownError");
        }
        node.end = end;
        node.raw = this.source.slice(start, end);
        this._node = node.parent;
    }
    onQuantifier(start, end, min, max, greedy) {
        const parent = this._node;
        if (parent.type !== "Alternative") {
            throw new Error("UnknownError");
        }
        const element = parent.elements.pop();
        if (element == null ||
            element.type === "Quantifier" ||
            (element.type === "Assertion" && element.kind !== "lookahead")) {
            throw new Error("UnknownError");
        }
        const node = {
            type: "Quantifier",
            parent,
            start: element.start,
            end,
            raw: this.source.slice(element.start, end),
            min,
            max,
            greedy,
            element,
        };
        parent.elements.push(node);
        element.parent = node;
    }
    onLookaroundAssertionEnter(start, kind, negate) {
        const parent = this._node;
        if (parent.type !== "Alternative") {
            throw new Error("UnknownError");
        }
        const node = (this._node = {
            type: "Assertion",
            parent,
            start,
            end: start,
            raw: "",
            kind,
            negate,
            alternatives: [],
        });
        parent.elements.push(node);
    }
    onLookaroundAssertionLeave(start, end) {
        const node = this._node;
        if (node.type !== "Assertion" || node.parent.type !== "Alternative") {
            throw new Error("UnknownError");
        }
        node.end = end;
        node.raw = this.source.slice(start, end);
        this._node = node.parent;
    }
    onEdgeAssertion(start, end, kind) {
        const parent = this._node;
        if (parent.type !== "Alternative") {
            throw new Error("UnknownError");
        }
        parent.elements.push({
            type: "Assertion",
            parent,
            start,
            end,
            raw: this.source.slice(start, end),
            kind,
        });
    }
    onWordBoundaryAssertion(start, end, kind, negate) {
        const parent = this._node;
        if (parent.type !== "Alternative") {
            throw new Error("UnknownError");
        }
        parent.elements.push({
            type: "Assertion",
            parent,
            start,
            end,
            raw: this.source.slice(start, end),
            kind,
            negate,
        });
    }
    onAnyCharacterSet(start, end, kind) {
        const parent = this._node;
        if (parent.type !== "Alternative") {
            throw new Error("UnknownError");
        }
        parent.elements.push({
            type: "CharacterSet",
            parent,
            start,
            end,
            raw: this.source.slice(start, end),
            kind,
        });
    }
    onEscapeCharacterSet(start, end, kind, negate) {
        const parent = this._node;
        if (parent.type !== "Alternative" && parent.type !== "CharacterClass") {
            throw new Error("UnknownError");
        }
        parent.elements.push({
            type: "CharacterSet",
            parent,
            start,
            end,
            raw: this.source.slice(start, end),
            kind,
            negate,
        });
    }
    onUnicodePropertyCharacterSet(start, end, kind, key, value, negate) {
        const parent = this._node;
        if (parent.type !== "Alternative" && parent.type !== "CharacterClass") {
            throw new Error("UnknownError");
        }
        parent.elements.push({
            type: "CharacterSet",
            parent,
            start,
            end,
            raw: this.source.slice(start, end),
            kind,
            key,
            value,
            negate,
        });
    }
    onCharacter(start, end, value) {
        const parent = this._node;
        if (parent.type !== "Alternative" && parent.type !== "CharacterClass") {
            throw new Error("UnknownError");
        }
        parent.elements.push({
            type: "Character",
            parent,
            start,
            end,
            raw: this.source.slice(start, end),
            value,
        });
    }
    onBackreference(start, end, ref) {
        const parent = this._node;
        if (parent.type !== "Alternative") {
            throw new Error("UnknownError");
        }
        const node = {
            type: "Backreference",
            parent,
            start,
            end,
            raw: this.source.slice(start, end),
            ref,
            resolved: DummyCapturingGroup,
        };
        parent.elements.push(node);
        this._backreferences.push(node);
    }
    onCharacterClassEnter(start, negate) {
        const parent = this._node;
        if (parent.type !== "Alternative") {
            throw new Error("UnknownError");
        }
        this._node = {
            type: "CharacterClass",
            parent,
            start,
            end: start,
            raw: "",
            negate,
            elements: [],
        };
        parent.elements.push(this._node);
    }
    onCharacterClassLeave(start, end) {
        const node = this._node;
        if (node.type !== "CharacterClass" ||
            node.parent.type !== "Alternative") {
            throw new Error("UnknownError");
        }
        node.end = end;
        node.raw = this.source.slice(start, end);
        this._node = node.parent;
    }
    onCharacterClassRange(start, end) {
        const parent = this._node;
        if (parent.type !== "CharacterClass") {
            throw new Error("UnknownError");
        }
        const elements = parent.elements;
        const max = elements.pop();
        const hyphen = elements.pop();
        const min = elements.pop();
        if (!min ||
            !max ||
            !hyphen ||
            min.type !== "Character" ||
            max.type !== "Character" ||
            hyphen.type !== "Character" ||
            hyphen.value !== HyphenMinus) {
            throw new Error("UnknownError");
        }
        const node = {
            type: "CharacterClassRange",
            parent,
            start,
            end,
            raw: this.source.slice(start, end),
            min,
            max,
        };
        min.parent = node;
        max.parent = node;
        elements.push(node);
    }
}
class RegExpParser {
    constructor(options) {
        this._state = new RegExpParserState(options);
        this._validator = new RegExpValidator(this._state);
    }
    parseLiteral(source, start = 0, end = source.length) {
        this._state.source = source;
        this._validator.validateLiteral(source, start, end);
        const pattern = this._state.pattern;
        const flags = this._state.flags;
        const literal = {
            type: "RegExpLiteral",
            parent: null,
            start,
            end,
            raw: source,
            pattern,
            flags,
        };
        pattern.parent = literal;
        flags.parent = literal;
        return literal;
    }
    parseFlags(source, start = 0, end = source.length) {
        this._state.source = source;
        this._validator.validateFlags(source, start, end);
        return this._state.flags;
    }
    parsePattern(source, start = 0, end = source.length, uFlag = false) {
        this._state.source = source;
        this._validator.validatePattern(source, start, end, uFlag);
        return this._state.pattern;
    }
}

class RegExpVisitor {
    constructor(handlers) {
        this._handlers = handlers;
    }
    visit(node) {
        switch (node.type) {
            case "Alternative":
                this.visitAlternative(node);
                break;
            case "Assertion":
                this.visitAssertion(node);
                break;
            case "Backreference":
                this.visitBackreference(node);
                break;
            case "CapturingGroup":
                this.visitCapturingGroup(node);
                break;
            case "Character":
                this.visitCharacter(node);
                break;
            case "CharacterClass":
                this.visitCharacterClass(node);
                break;
            case "CharacterClassRange":
                this.visitCharacterClassRange(node);
                break;
            case "CharacterSet":
                this.visitCharacterSet(node);
                break;
            case "Flags":
                this.visitFlags(node);
                break;
            case "Group":
                this.visitGroup(node);
                break;
            case "Pattern":
                this.visitPattern(node);
                break;
            case "Quantifier":
                this.visitQuantifier(node);
                break;
            case "RegExpLiteral":
                this.visitRegExpLiteral(node);
                break;
            default:
                throw new Error(`Unknown type: ${node.type}`);
        }
    }
    visitAlternative(node) {
        if (this._handlers.onAlternativeEnter) {
            this._handlers.onAlternativeEnter(node);
        }
        node.elements.forEach(this.visit, this);
        if (this._handlers.onAlternativeLeave) {
            this._handlers.onAlternativeLeave(node);
        }
    }
    visitAssertion(node) {
        if (this._handlers.onAssertionEnter) {
            this._handlers.onAssertionEnter(node);
        }
        if (node.kind === "lookahead" || node.kind === "lookbehind") {
            node.alternatives.forEach(this.visit, this);
        }
        if (this._handlers.onAssertionLeave) {
            this._handlers.onAssertionLeave(node);
        }
    }
    visitBackreference(node) {
        if (this._handlers.onBackreferenceEnter) {
            this._handlers.onBackreferenceEnter(node);
        }
        if (this._handlers.onBackreferenceLeave) {
            this._handlers.onBackreferenceLeave(node);
        }
    }
    visitCapturingGroup(node) {
        if (this._handlers.onCapturingGroupEnter) {
            this._handlers.onCapturingGroupEnter(node);
        }
        node.alternatives.forEach(this.visit, this);
        if (this._handlers.onCapturingGroupLeave) {
            this._handlers.onCapturingGroupLeave(node);
        }
    }
    visitCharacter(node) {
        if (this._handlers.onCharacterEnter) {
            this._handlers.onCharacterEnter(node);
        }
        if (this._handlers.onCharacterLeave) {
            this._handlers.onCharacterLeave(node);
        }
    }
    visitCharacterClass(node) {
        if (this._handlers.onCharacterClassEnter) {
            this._handlers.onCharacterClassEnter(node);
        }
        node.elements.forEach(this.visit, this);
        if (this._handlers.onCharacterClassLeave) {
            this._handlers.onCharacterClassLeave(node);
        }
    }
    visitCharacterClassRange(node) {
        if (this._handlers.onCharacterClassRangeEnter) {
            this._handlers.onCharacterClassRangeEnter(node);
        }
        this.visitCharacter(node.min);
        this.visitCharacter(node.max);
        if (this._handlers.onCharacterClassRangeLeave) {
            this._handlers.onCharacterClassRangeLeave(node);
        }
    }
    visitCharacterSet(node) {
        if (this._handlers.onCharacterSetEnter) {
            this._handlers.onCharacterSetEnter(node);
        }
        if (this._handlers.onCharacterSetLeave) {
            this._handlers.onCharacterSetLeave(node);
        }
    }
    visitFlags(node) {
        if (this._handlers.onFlagsEnter) {
            this._handlers.onFlagsEnter(node);
        }
        if (this._handlers.onFlagsLeave) {
            this._handlers.onFlagsLeave(node);
        }
    }
    visitGroup(node) {
        if (this._handlers.onGroupEnter) {
            this._handlers.onGroupEnter(node);
        }
        node.alternatives.forEach(this.visit, this);
        if (this._handlers.onGroupLeave) {
            this._handlers.onGroupLeave(node);
        }
    }
    visitPattern(node) {
        if (this._handlers.onPatternEnter) {
            this._handlers.onPatternEnter(node);
        }
        node.alternatives.forEach(this.visit, this);
        if (this._handlers.onPatternLeave) {
            this._handlers.onPatternLeave(node);
        }
    }
    visitQuantifier(node) {
        if (this._handlers.onQuantifierEnter) {
            this._handlers.onQuantifierEnter(node);
        }
        this.visit(node.element);
        if (this._handlers.onQuantifierLeave) {
            this._handlers.onQuantifierLeave(node);
        }
    }
    visitRegExpLiteral(node) {
        if (this._handlers.onRegExpLiteralEnter) {
            this._handlers.onRegExpLiteralEnter(node);
        }
        this.visitPattern(node.pattern);
        this.visitFlags(node.flags);
        if (this._handlers.onRegExpLiteralLeave) {
            this._handlers.onRegExpLiteralLeave(node);
        }
    }
}

function parseRegExpLiteral(source, options) {
    return new RegExpParser(options).parseLiteral(String(source));
}
function validateRegExpLiteral(source, options) {
    return new RegExpValidator(options).validateLiteral(source);
}
function visitRegExpAST(node, handlers) {
    new RegExpVisitor(handlers).visit(node);
}

export { ast as AST, RegExpParser, RegExpValidator, parseRegExpLiteral, validateRegExpLiteral, visitRegExpAST };
//# sourceMappingURL=index.mjs.map
