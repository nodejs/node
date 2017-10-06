/*
 * The Original Code is Mozilla Universal charset detector code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   António Afonso (antonio.afonso gmail.com) - port to JavaScript
 *   Mark Pilgrim - port to Python
 *   Shy Shalom - original C code
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

!function(jschardet) {

var consts = jschardet.Constants;

// BIG5

jschardet.BIG5_cls = [
    1,1,1,1,1,1,1,1,  // 00 - 07    //allow 0x00 as legal value
    1,1,1,1,1,1,0,0,  // 08 - 0f
    1,1,1,1,1,1,1,1,  // 10 - 17
    1,1,1,0,1,1,1,1,  // 18 - 1f
    1,1,1,1,1,1,1,1,  // 20 - 27
    1,1,1,1,1,1,1,1,  // 28 - 2f
    1,1,1,1,1,1,1,1,  // 30 - 37
    1,1,1,1,1,1,1,1,  // 38 - 3f
    2,2,2,2,2,2,2,2,  // 40 - 47
    2,2,2,2,2,2,2,2,  // 48 - 4f
    2,2,2,2,2,2,2,2,  // 50 - 57
    2,2,2,2,2,2,2,2,  // 58 - 5f
    2,2,2,2,2,2,2,2,  // 60 - 67
    2,2,2,2,2,2,2,2,  // 68 - 6f
    2,2,2,2,2,2,2,2,  // 70 - 77
    2,2,2,2,2,2,2,1,  // 78 - 7f
    4,4,4,4,4,4,4,4,  // 80 - 87
    4,4,4,4,4,4,4,4,  // 88 - 8f
    4,4,4,4,4,4,4,4,  // 90 - 97
    4,4,4,4,4,4,4,4,  // 98 - 9f
    4,3,3,3,3,3,3,3,  // a0 - a7
    3,3,3,3,3,3,3,3,  // a8 - af
    3,3,3,3,3,3,3,3,  // b0 - b7
    3,3,3,3,3,3,3,3,  // b8 - bf
    3,3,3,3,3,3,3,3,  // c0 - c7
    3,3,3,3,3,3,3,3,  // c8 - cf
    3,3,3,3,3,3,3,3,  // d0 - d7
    3,3,3,3,3,3,3,3,  // d8 - df
    3,3,3,3,3,3,3,3,  // e0 - e7
    3,3,3,3,3,3,3,3,  // e8 - ef
    3,3,3,3,3,3,3,3,  // f0 - f7
    3,3,3,3,3,3,3,0   // f8 - ff
];

jschardet.BIG5_st = [
    consts.error,consts.start,consts.start,    3,consts.error,consts.error,consts.error,consts.error, //00-07
    consts.error,consts.error,consts.itsMe,consts.itsMe,consts.itsMe,consts.itsMe,consts.itsMe,consts.error, //08-0f
    consts.error,consts.start,consts.start,consts.start,consts.start,consts.start,consts.start,consts.start  //10-17
];

jschardet.Big5CharLenTable = [0, 1, 1, 2, 0];

jschardet.Big5SMModel = {
    "classTable"    : jschardet.BIG5_cls,
    "classFactor"   : 5,
    "stateTable"    : jschardet.BIG5_st,
    "charLenTable"  : jschardet.Big5CharLenTable,
    "name"          : "Big5"
};

// EUC-JP

jschardet.EUCJP_cls = [
    4,4,4,4,4,4,4,4,  // 00 - 07
    4,4,4,4,4,4,5,5,  // 08 - 0f
    4,4,4,4,4,4,4,4,  // 10 - 17
    4,4,4,5,4,4,4,4,  // 18 - 1f
    4,4,4,4,4,4,4,4,  // 20 - 27
    4,4,4,4,4,4,4,4,  // 28 - 2f
    4,4,4,4,4,4,4,4,  // 30 - 37
    4,4,4,4,4,4,4,4,  // 38 - 3f
    4,4,4,4,4,4,4,4,  // 40 - 47
    4,4,4,4,4,4,4,4,  // 48 - 4f
    4,4,4,4,4,4,4,4,  // 50 - 57
    4,4,4,4,4,4,4,4,  // 58 - 5f
    4,4,4,4,4,4,4,4,  // 60 - 67
    4,4,4,4,4,4,4,4,  // 68 - 6f
    4,4,4,4,4,4,4,4,  // 70 - 77
    4,4,4,4,4,4,4,4,  // 78 - 7f
    5,5,5,5,5,5,5,5,  // 80 - 87
    5,5,5,5,5,5,1,3,  // 88 - 8f
    5,5,5,5,5,5,5,5,  // 90 - 97
    5,5,5,5,5,5,5,5,  // 98 - 9f
    5,2,2,2,2,2,2,2,  // a0 - a7
    2,2,2,2,2,2,2,2,  // a8 - af
    2,2,2,2,2,2,2,2,  // b0 - b7
    2,2,2,2,2,2,2,2,  // b8 - bf
    2,2,2,2,2,2,2,2,  // c0 - c7
    2,2,2,2,2,2,2,2,  // c8 - cf
    2,2,2,2,2,2,2,2,  // d0 - d7
    2,2,2,2,2,2,2,2,  // d8 - df
    0,0,0,0,0,0,0,0,  // e0 - e7
    0,0,0,0,0,0,0,0,  // e8 - ef
    0,0,0,0,0,0,0,0,  // f0 - f7
    0,0,0,0,0,0,0,5   // f8 - ff
];

jschardet.EUCJP_st = [
         3,    4,    3,    5,consts.start,consts.error,consts.error,consts.error, //00-07
     consts.error,consts.error,consts.error,consts.error,consts.itsMe,consts.itsMe,consts.itsMe,consts.itsMe, //08-0f
     consts.itsMe,consts.itsMe,consts.start,consts.error,consts.start,consts.error,consts.error,consts.error, //10-17
     consts.error,consts.error,consts.start,consts.error,consts.error,consts.error,    3,consts.error, //18-1f
         3,consts.error,consts.error,consts.error,consts.start,consts.start,consts.start,consts.start  //20-27
];

jschardet.EUCJPCharLenTable = [2, 2, 2, 3, 1, 0];

jschardet.EUCJPSMModel = {
    "classTable"    : jschardet.EUCJP_cls,
    "classFactor"   : 6,
    "stateTable"    : jschardet.EUCJP_st,
    "charLenTable"  : jschardet.EUCJPCharLenTable,
    "name"          : "EUC-JP"
};

// EUC-KR

jschardet.EUCKR_cls  = [
    1,1,1,1,1,1,1,1,  // 00 - 07
    1,1,1,1,1,1,0,0,  // 08 - 0f
    1,1,1,1,1,1,1,1,  // 10 - 17
    1,1,1,0,1,1,1,1,  // 18 - 1f
    1,1,1,1,1,1,1,1,  // 20 - 27
    1,1,1,1,1,1,1,1,  // 28 - 2f
    1,1,1,1,1,1,1,1,  // 30 - 37
    1,1,1,1,1,1,1,1,  // 38 - 3f
    1,1,1,1,1,1,1,1,  // 40 - 47
    1,1,1,1,1,1,1,1,  // 48 - 4f
    1,1,1,1,1,1,1,1,  // 50 - 57
    1,1,1,1,1,1,1,1,  // 58 - 5f
    1,1,1,1,1,1,1,1,  // 60 - 67
    1,1,1,1,1,1,1,1,  // 68 - 6f
    1,1,1,1,1,1,1,1,  // 70 - 77
    1,1,1,1,1,1,1,1,  // 78 - 7f
    0,0,0,0,0,0,0,0,  // 80 - 87
    0,0,0,0,0,0,0,0,  // 88 - 8f
    0,0,0,0,0,0,0,0,  // 90 - 97
    0,0,0,0,0,0,0,0,  // 98 - 9f
    0,2,2,2,2,2,2,2,  // a0 - a7
    2,2,2,2,2,3,3,3,  // a8 - af
    2,2,2,2,2,2,2,2,  // b0 - b7
    2,2,2,2,2,2,2,2,  // b8 - bf
    2,2,2,2,2,2,2,2,  // c0 - c7
    2,3,2,2,2,2,2,2,  // c8 - cf
    2,2,2,2,2,2,2,2,  // d0 - d7
    2,2,2,2,2,2,2,2,  // d8 - df
    2,2,2,2,2,2,2,2,  // e0 - e7
    2,2,2,2,2,2,2,2,  // e8 - ef
    2,2,2,2,2,2,2,2,  // f0 - f7
    2,2,2,2,2,2,2,0   // f8 - ff
];

jschardet.EUCKR_st = [
    consts.error,consts.start,    3,consts.error,consts.error,consts.error,consts.error,consts.error, //00-07
    consts.itsMe,consts.itsMe,consts.itsMe,consts.itsMe,consts.error,consts.error,consts.start,consts.start  //08-0f
];

jschardet.EUCKRCharLenTable = [0, 1, 2, 0];

jschardet.EUCKRSMModel = {
    "classTable"    : jschardet.EUCKR_cls,
    "classFactor"   : 4,
    "stateTable"    : jschardet.EUCKR_st,
    "charLenTable"  : jschardet.EUCKRCharLenTable,
    "name"          : "EUC-KR"
};

// EUC-TW

jschardet.EUCTW_cls = [
    2,2,2,2,2,2,2,2,  // 00 - 07
    2,2,2,2,2,2,0,0,  // 08 - 0f
    2,2,2,2,2,2,2,2,  // 10 - 17
    2,2,2,0,2,2,2,2,  // 18 - 1f
    2,2,2,2,2,2,2,2,  // 20 - 27
    2,2,2,2,2,2,2,2,  // 28 - 2f
    2,2,2,2,2,2,2,2,  // 30 - 37
    2,2,2,2,2,2,2,2,  // 38 - 3f
    2,2,2,2,2,2,2,2,  // 40 - 47
    2,2,2,2,2,2,2,2,  // 48 - 4f
    2,2,2,2,2,2,2,2,  // 50 - 57
    2,2,2,2,2,2,2,2,  // 58 - 5f
    2,2,2,2,2,2,2,2,  // 60 - 67
    2,2,2,2,2,2,2,2,  // 68 - 6f
    2,2,2,2,2,2,2,2,  // 70 - 77
    2,2,2,2,2,2,2,2,  // 78 - 7f
    0,0,0,0,0,0,0,0,  // 80 - 87
    0,0,0,0,0,0,6,0,  // 88 - 8f
    0,0,0,0,0,0,0,0,  // 90 - 97
    0,0,0,0,0,0,0,0,  // 98 - 9f
    0,3,4,4,4,4,4,4,  // a0 - a7
    5,5,1,1,1,1,1,1,  // a8 - af
    1,1,1,1,1,1,1,1,  // b0 - b7
    1,1,1,1,1,1,1,1,  // b8 - bf
    1,1,3,1,3,3,3,3,  // c0 - c7
    3,3,3,3,3,3,3,3,  // c8 - cf
    3,3,3,3,3,3,3,3,  // d0 - d7
    3,3,3,3,3,3,3,3,  // d8 - df
    3,3,3,3,3,3,3,3,  // e0 - e7
    3,3,3,3,3,3,3,3,  // e8 - ef
    3,3,3,3,3,3,3,3,  // f0 - f7
    3,3,3,3,3,3,3,0   // f8 - ff
];

jschardet.EUCTW_st = [
    consts.error,consts.error,consts.start,    3,    3,    3,    4,consts.error, //00-07
    consts.error,consts.error,consts.error,consts.error,consts.error,consts.error,consts.itsMe,consts.itsMe, //08-0f
    consts.itsMe,consts.itsMe,consts.itsMe,consts.itsMe,consts.itsMe,consts.error,consts.start,consts.error, //10-17
    consts.start,consts.start,consts.start,consts.error,consts.error,consts.error,consts.error,consts.error, //18-1f
        5,consts.error,consts.error,consts.error,consts.start,consts.error,consts.start,consts.start, //20-27
    consts.start,consts.error,consts.start,consts.start,consts.start,consts.start,consts.start,consts.start  //28-2f
];

jschardet.EUCTWCharLenTable = [0, 0, 1, 2, 2, 2, 3];

jschardet.EUCTWSMModel = {
    "classTable"    : jschardet.EUCTW_cls,
    "classFactor"   : 7,
    "stateTable"    : jschardet.EUCTW_st,
    "charLenTable"  : jschardet.EUCTWCharLenTable,
    "name"          : "x-euc-tw"
};

// GB2312

jschardet.GB2312_cls = [
    1,1,1,1,1,1,1,1,  // 00 - 07
    1,1,1,1,1,1,0,0,  // 08 - 0f
    1,1,1,1,1,1,1,1,  // 10 - 17
    1,1,1,0,1,1,1,1,  // 18 - 1f
    1,1,1,1,1,1,1,1,  // 20 - 27
    1,1,1,1,1,1,1,1,  // 28 - 2f
    3,3,3,3,3,3,3,3,  // 30 - 37
    3,3,1,1,1,1,1,1,  // 38 - 3f
    2,2,2,2,2,2,2,2,  // 40 - 47
    2,2,2,2,2,2,2,2,  // 48 - 4f
    2,2,2,2,2,2,2,2,  // 50 - 57
    2,2,2,2,2,2,2,2,  // 58 - 5f
    2,2,2,2,2,2,2,2,  // 60 - 67
    2,2,2,2,2,2,2,2,  // 68 - 6f
    2,2,2,2,2,2,2,2,  // 70 - 77
    2,2,2,2,2,2,2,4,  // 78 - 7f
    5,6,6,6,6,6,6,6,  // 80 - 87
    6,6,6,6,6,6,6,6,  // 88 - 8f
    6,6,6,6,6,6,6,6,  // 90 - 97
    6,6,6,6,6,6,6,6,  // 98 - 9f
    6,6,6,6,6,6,6,6,  // a0 - a7
    6,6,6,6,6,6,6,6,  // a8 - af
    6,6,6,6,6,6,6,6,  // b0 - b7
    6,6,6,6,6,6,6,6,  // b8 - bf
    6,6,6,6,6,6,6,6,  // c0 - c7
    6,6,6,6,6,6,6,6,  // c8 - cf
    6,6,6,6,6,6,6,6,  // d0 - d7
    6,6,6,6,6,6,6,6,  // d8 - df
    6,6,6,6,6,6,6,6,  // e0 - e7
    6,6,6,6,6,6,6,6,  // e8 - ef
    6,6,6,6,6,6,6,6,  // f0 - f7
    6,6,6,6,6,6,6,0   // f8 - ff
];

jschardet.GB2312_st = [
    consts.error,consts.start,consts.start,consts.start,consts.start,consts.start,    3,consts.error, //00-07
    consts.error,consts.error,consts.error,consts.error,consts.error,consts.error,consts.itsMe,consts.itsMe, //08-0f
    consts.itsMe,consts.itsMe,consts.itsMe,consts.itsMe,consts.itsMe,consts.error,consts.error,consts.start, //10-17
        4,consts.error,consts.start,consts.start,consts.error,consts.error,consts.error,consts.error, //18-1f
    consts.error,consts.error,    5,consts.error,consts.error,consts.error,consts.itsMe,consts.error, //20-27
    consts.error,consts.error,consts.start,consts.start,consts.start,consts.start,consts.start,consts.start  //28-2f
];

// To be accurate, the length of class 6 can be either 2 or 4.
// But it is not necessary to discriminate between the two since
// it is used for frequency analysis only, and we are validing
// each code range there as well. So it is safe to set it to be
// 2 here.
jschardet.GB2312CharLenTable = [0, 1, 1, 1, 1, 1, 2];

jschardet.GB2312SMModel = {
    "classTable"    : jschardet.GB2312_cls,
    "classFactor"   : 7,
    "stateTable"    : jschardet.GB2312_st,
    "charLenTable"  : jschardet.GB2312CharLenTable,
    "name"          : "GB2312"
};

// Shift_JIS

jschardet.SJIS_cls = [
    1,1,1,1,1,1,1,1,  // 00 - 07
    1,1,1,1,1,1,0,0,  // 08 - 0f
    1,1,1,1,1,1,1,1,  // 10 - 17
    1,1,1,0,1,1,1,1,  // 18 - 1f
    1,1,1,1,1,1,1,1,  // 20 - 27
    1,1,1,1,1,1,1,1,  // 28 - 2f
    1,1,1,1,1,1,1,1,  // 30 - 37
    1,1,1,1,1,1,1,1,  // 38 - 3f
    2,2,2,2,2,2,2,2,  // 40 - 47
    2,2,2,2,2,2,2,2,  // 48 - 4f
    2,2,2,2,2,2,2,2,  // 50 - 57
    2,2,2,2,2,2,2,2,  // 58 - 5f
    2,2,2,2,2,2,2,2,  // 60 - 67
    2,2,2,2,2,2,2,2,  // 68 - 6f
    2,2,2,2,2,2,2,2,  // 70 - 77
    2,2,2,2,2,2,2,1,  // 78 - 7f
    3,3,3,3,3,3,3,3,  // 80 - 87
    3,3,3,3,3,3,3,3,  // 88 - 8f
    3,3,3,3,3,3,3,3,  // 90 - 97
    3,3,3,3,3,3,3,3,  // 98 - 9f
    // 0xa0 is illegal in sjis encoding, but some pages does
    // contain such byte. We need to be more consts.error forgiven.
    2,2,2,2,2,2,2,2,  // a0 - a7
    2,2,2,2,2,2,2,2,  // a8 - af
    2,2,2,2,2,2,2,2,  // b0 - b7
    2,2,2,2,2,2,2,2,  // b8 - bf
    2,2,2,2,2,2,2,2,  // c0 - c7
    2,2,2,2,2,2,2,2,  // c8 - cf
    2,2,2,2,2,2,2,2,  // d0 - d7
    2,2,2,2,2,2,2,2,  // d8 - df
    3,3,3,3,3,3,3,3,  // e0 - e7
    3,3,3,3,3,4,4,4,  // e8 - ef
    3,3,3,3,3,3,3,3,  // f0 - f7
    3,3,3,3,3,0,0,0   // f8 - ff
];

jschardet.SJIS_st = [
    consts.error,consts.start,consts.start,    3,consts.error,consts.error,consts.error,consts.error, //00-07
    consts.error,consts.error,consts.error,consts.error,consts.itsMe,consts.itsMe,consts.itsMe,consts.itsMe, //08-0f
    consts.itsMe,consts.itsMe,consts.error,consts.error,consts.start,consts.start,consts.start,consts.start  //10-17
];

jschardet.SJISCharLenTable = [0, 1, 1, 2, 0, 0];

jschardet.SJISSMModel = {
    "classTable"    : jschardet.SJIS_cls,
    "classFactor"   : 6,
    "stateTable"    : jschardet.SJIS_st,
    "charLenTable"  : jschardet.SJISCharLenTable,
    "name"          : "Shift_JIS"
};

//UCS2-BE

jschardet.UCS2BE_cls = [
    0,0,0,0,0,0,0,0,  // 00 - 07
    0,0,1,0,0,2,0,0,  // 08 - 0f
    0,0,0,0,0,0,0,0,  // 10 - 17
    0,0,0,3,0,0,0,0,  // 18 - 1f
    0,0,0,0,0,0,0,0,  // 20 - 27
    0,3,3,3,3,3,0,0,  // 28 - 2f
    0,0,0,0,0,0,0,0,  // 30 - 37
    0,0,0,0,0,0,0,0,  // 38 - 3f
    0,0,0,0,0,0,0,0,  // 40 - 47
    0,0,0,0,0,0,0,0,  // 48 - 4f
    0,0,0,0,0,0,0,0,  // 50 - 57
    0,0,0,0,0,0,0,0,  // 58 - 5f
    0,0,0,0,0,0,0,0,  // 60 - 67
    0,0,0,0,0,0,0,0,  // 68 - 6f
    0,0,0,0,0,0,0,0,  // 70 - 77
    0,0,0,0,0,0,0,0,  // 78 - 7f
    0,0,0,0,0,0,0,0,  // 80 - 87
    0,0,0,0,0,0,0,0,  // 88 - 8f
    0,0,0,0,0,0,0,0,  // 90 - 97
    0,0,0,0,0,0,0,0,  // 98 - 9f
    0,0,0,0,0,0,0,0,  // a0 - a7
    0,0,0,0,0,0,0,0,  // a8 - af
    0,0,0,0,0,0,0,0,  // b0 - b7
    0,0,0,0,0,0,0,0,  // b8 - bf
    0,0,0,0,0,0,0,0,  // c0 - c7
    0,0,0,0,0,0,0,0,  // c8 - cf
    0,0,0,0,0,0,0,0,  // d0 - d7
    0,0,0,0,0,0,0,0,  // d8 - df
    0,0,0,0,0,0,0,0,  // e0 - e7
    0,0,0,0,0,0,0,0,  // e8 - ef
    0,0,0,0,0,0,0,0,  // f0 - f7
    0,0,0,0,0,0,4,5   // f8 - ff
];

jschardet.UCS2BE_st  = [
         5,    7,    7,consts.error,    4,    3,consts.error,consts.error, //00-07
     consts.error,consts.error,consts.error,consts.error,consts.itsMe,consts.itsMe,consts.itsMe,consts.itsMe, //08-0f
     consts.itsMe,consts.itsMe,    6,    6,    6,    6,consts.error,consts.error, //10-17
         6,    6,    6,    6,    6,consts.itsMe,    6,    6, //18-1f
         6,    6,    6,    6,    5,    7,    7,consts.error, //20-27
         5,    8,    6,    6,consts.error,    6,    6,    6, //28-2f
         6,    6,    6,    6,consts.error,consts.error,consts.start,consts.start  //30-37
];

jschardet.UCS2BECharLenTable = [2, 2, 2, 0, 2, 2];

jschardet.UCS2BESMModel = {
    "classTable"    : jschardet.UCS2BE_cls,
    "classFactor"   : 6,
    "stateTable"    : jschardet.UCS2BE_st,
    "charLenTable"  : jschardet.UCS2BECharLenTable,
    "name"          : "UTF-16BE"
};

// UCS2-LE

jschardet.UCS2LE_cls = [
    0,0,0,0,0,0,0,0,  // 00 - 07
    0,0,1,0,0,2,0,0,  // 08 - 0f
    0,0,0,0,0,0,0,0,  // 10 - 17
    0,0,0,3,0,0,0,0,  // 18 - 1f
    0,0,0,0,0,0,0,0,  // 20 - 27
    0,3,3,3,3,3,0,0,  // 28 - 2f
    0,0,0,0,0,0,0,0,  // 30 - 37
    0,0,0,0,0,0,0,0,  // 38 - 3f
    0,0,0,0,0,0,0,0,  // 40 - 47
    0,0,0,0,0,0,0,0,  // 48 - 4f
    0,0,0,0,0,0,0,0,  // 50 - 57
    0,0,0,0,0,0,0,0,  // 58 - 5f
    0,0,0,0,0,0,0,0,  // 60 - 67
    0,0,0,0,0,0,0,0,  // 68 - 6f
    0,0,0,0,0,0,0,0,  // 70 - 77
    0,0,0,0,0,0,0,0,  // 78 - 7f
    0,0,0,0,0,0,0,0,  // 80 - 87
    0,0,0,0,0,0,0,0,  // 88 - 8f
    0,0,0,0,0,0,0,0,  // 90 - 97
    0,0,0,0,0,0,0,0,  // 98 - 9f
    0,0,0,0,0,0,0,0,  // a0 - a7
    0,0,0,0,0,0,0,0,  // a8 - af
    0,0,0,0,0,0,0,0,  // b0 - b7
    0,0,0,0,0,0,0,0,  // b8 - bf
    0,0,0,0,0,0,0,0,  // c0 - c7
    0,0,0,0,0,0,0,0,  // c8 - cf
    0,0,0,0,0,0,0,0,  // d0 - d7
    0,0,0,0,0,0,0,0,  // d8 - df
    0,0,0,0,0,0,0,0,  // e0 - e7
    0,0,0,0,0,0,0,0,  // e8 - ef
    0,0,0,0,0,0,0,0,  // f0 - f7
    0,0,0,0,0,0,4,5   // f8 - ff
];

jschardet.UCS2LE_st = [
         6,    6,    7,    6,    4,    3,consts.error,consts.error, //00-07
     consts.error,consts.error,consts.error,consts.error,consts.itsMe,consts.itsMe,consts.itsMe,consts.itsMe, //08-0f
     consts.itsMe,consts.itsMe,    5,    5,    5,consts.error,consts.itsMe,consts.error, //10-17
         5,    5,    5,consts.error,    5,consts.error,    6,    6, //18-1f
         7,    6,    8,    8,    5,    5,    5,consts.error, //20-27
         5,    5,    5,consts.error,consts.error,consts.error,    5,    5, //28-2f
         5,    5,    5,consts.error,    5,consts.error,consts.start,consts.start  //30-37
];

jschardet.UCS2LECharLenTable = [2, 2, 2, 2, 2, 2];

jschardet.UCS2LESMModel = {
    "classTable"    : jschardet.UCS2LE_cls,
    "classFactor"   : 6,
    "stateTable"    : jschardet.UCS2LE_st,
    "charLenTable"  : jschardet.UCS2LECharLenTable,
    "name"          : "UTF-16LE"
};

// UTF-8

jschardet.UTF8_cls = [
    1,1,1,1,1,1,1,1,  // 00 - 07  //allow 0x00 as a legal value
    1,1,1,1,1,1,0,0,  // 08 - 0f
    1,1,1,1,1,1,1,1,  // 10 - 17
    1,1,1,0,1,1,1,1,  // 18 - 1f
    1,1,1,1,1,1,1,1,  // 20 - 27
    1,1,1,1,1,1,1,1,  // 28 - 2f
    1,1,1,1,1,1,1,1,  // 30 - 37
    1,1,1,1,1,1,1,1,  // 38 - 3f
    1,1,1,1,1,1,1,1,  // 40 - 47
    1,1,1,1,1,1,1,1,  // 48 - 4f
    1,1,1,1,1,1,1,1,  // 50 - 57
    1,1,1,1,1,1,1,1,  // 58 - 5f
    1,1,1,1,1,1,1,1,  // 60 - 67
    1,1,1,1,1,1,1,1,  // 68 - 6f
    1,1,1,1,1,1,1,1,  // 70 - 77
    1,1,1,1,1,1,1,1,  // 78 - 7f
    2,2,2,2,3,3,3,3,  // 80 - 87
    4,4,4,4,4,4,4,4,  // 88 - 8f
    4,4,4,4,4,4,4,4,  // 90 - 97
    4,4,4,4,4,4,4,4,  // 98 - 9f
    5,5,5,5,5,5,5,5,  // a0 - a7
    5,5,5,5,5,5,5,5,  // a8 - af
    5,5,5,5,5,5,5,5,  // b0 - b7
    5,5,5,5,5,5,5,5,  // b8 - bf
    0,0,6,6,6,6,6,6,  // c0 - c7
    6,6,6,6,6,6,6,6,  // c8 - cf
    6,6,6,6,6,6,6,6,  // d0 - d7
    6,6,6,6,6,6,6,6,  // d8 - df
    7,8,8,8,8,8,8,8,  // e0 - e7
    8,8,8,8,8,9,8,8,  // e8 - ef
    10,11,11,11,11,11,11,11,  // f0 - f7
    12,13,13,13,14,15,0,0    // f8 - ff
];

jschardet.UTF8_st = [
    consts.error,consts.start,consts.error,consts.error,consts.error,consts.error,    12,  10, //00-07
        9,    11,    8,    7,    6,    5,    4,   3, //08-0f
    consts.error,consts.error,consts.error,consts.error,consts.error,consts.error,consts.error,consts.error, //10-17
    consts.error,consts.error,consts.error,consts.error,consts.error,consts.error,consts.error,consts.error, //18-1f
    consts.itsMe,consts.itsMe,consts.itsMe,consts.itsMe,consts.itsMe,consts.itsMe,consts.itsMe,consts.itsMe, //20-27
    consts.itsMe,consts.itsMe,consts.itsMe,consts.itsMe,consts.itsMe,consts.itsMe,consts.itsMe,consts.itsMe, //28-2f
    consts.error,consts.error,    5,    5,    5,    5,consts.error,consts.error, //30-37
    consts.error,consts.error,consts.error,consts.error,consts.error,consts.error,consts.error,consts.error, //38-3f
    consts.error,consts.error,consts.error,    5,    5,    5,consts.error,consts.error, //40-47
    consts.error,consts.error,consts.error,consts.error,consts.error,consts.error,consts.error,consts.error, //48-4f
    consts.error,consts.error,    7,    7,    7,    7,consts.error,consts.error, //50-57
    consts.error,consts.error,consts.error,consts.error,consts.error,consts.error,consts.error,consts.error, //58-5f
    consts.error,consts.error,consts.error,consts.error,    7,    7,consts.error,consts.error, //60-67
    consts.error,consts.error,consts.error,consts.error,consts.error,consts.error,consts.error,consts.error, //68-6f
    consts.error,consts.error,    9,    9,    9,    9,consts.error,consts.error, //70-77
    consts.error,consts.error,consts.error,consts.error,consts.error,consts.error,consts.error,consts.error, //78-7f
    consts.error,consts.error,consts.error,consts.error,consts.error,    9,consts.error,consts.error, //80-87
    consts.error,consts.error,consts.error,consts.error,consts.error,consts.error,consts.error,consts.error, //88-8f
    consts.error,consts.error,   12,   12,   12,   12,consts.error,consts.error, //90-97
    consts.error,consts.error,consts.error,consts.error,consts.error,consts.error,consts.error,consts.error, //98-9f
    consts.error,consts.error,consts.error,consts.error,consts.error,   12,consts.error,consts.error, //a0-a7
    consts.error,consts.error,consts.error,consts.error,consts.error,consts.error,consts.error,consts.error, //a8-af
    consts.error,consts.error,   12,   12,   12,consts.error,consts.error,consts.error, //b0-b7
    consts.error,consts.error,consts.error,consts.error,consts.error,consts.error,consts.error,consts.error, //b8-bf
    consts.error,consts.error,consts.start,consts.start,consts.start,consts.start,consts.error,consts.error, //c0-c7
    consts.error,consts.error,consts.error,consts.error,consts.error,consts.error,consts.error,consts.error  //c8-cf
];

jschardet.UTF8CharLenTable = [0, 1, 0, 0, 0, 0, 2, 3, 3, 3, 4, 4, 5, 5, 6, 6];

jschardet.UTF8SMModel = {
    "classTable"    : jschardet.UTF8_cls,
    "classFactor"   : 16,
    "stateTable"    : jschardet.UTF8_st,
    "charLenTable"  : jschardet.UTF8CharLenTable,
    "name"          : "UTF-8"
};

}(require('./init'));
