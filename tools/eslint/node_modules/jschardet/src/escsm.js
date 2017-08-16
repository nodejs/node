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

jschardet.HZ_cls = [
    1,0,0,0,0,0,0,0,  // 00 - 07
    0,0,0,0,0,0,0,0,  // 08 - 0f
    0,0,0,0,0,0,0,0,  // 10 - 17
    0,0,0,1,0,0,0,0,  // 18 - 1f
    0,0,0,0,0,0,0,0,  // 20 - 27
    0,0,0,0,0,0,0,0,  // 28 - 2f
    0,0,0,0,0,0,0,0,  // 30 - 37
    0,0,0,0,0,0,0,0,  // 38 - 3f
    0,0,0,0,0,0,0,0,  // 40 - 47
    0,0,0,0,0,0,0,0,  // 48 - 4f
    0,0,0,0,0,0,0,0,  // 50 - 57
    0,0,0,0,0,0,0,0,  // 58 - 5f
    0,0,0,0,0,0,0,0,  // 60 - 67
    0,0,0,0,0,0,0,0,  // 68 - 6f
    0,0,0,0,0,0,0,0,  // 70 - 77
    0,0,0,4,0,5,2,0,  // 78 - 7f
    1,1,1,1,1,1,1,1,  // 80 - 87
    1,1,1,1,1,1,1,1,  // 88 - 8f
    1,1,1,1,1,1,1,1,  // 90 - 97
    1,1,1,1,1,1,1,1,  // 98 - 9f
    1,1,1,1,1,1,1,1,  // a0 - a7
    1,1,1,1,1,1,1,1,  // a8 - af
    1,1,1,1,1,1,1,1,  // b0 - b7
    1,1,1,1,1,1,1,1,  // b8 - bf
    1,1,1,1,1,1,1,1,  // c0 - c7
    1,1,1,1,1,1,1,1,  // c8 - cf
    1,1,1,1,1,1,1,1,  // d0 - d7
    1,1,1,1,1,1,1,1,  // d8 - df
    1,1,1,1,1,1,1,1,  // e0 - e7
    1,1,1,1,1,1,1,1,  // e8 - ef
    1,1,1,1,1,1,1,1,  // f0 - f7
    1,1,1,1,1,1,1,1   // f8 - ff
];

jschardet.HZ_st = [
    consts.start,consts.error,    3,consts.start,consts.start,consts.start,consts.error,consts.error, // 00-07
    consts.error,consts.error,consts.error,consts.error,consts.itsMe,consts.itsMe,consts.itsMe,consts.itsMe, // 08-0f
    consts.itsMe,consts.itsMe,consts.error,consts.error,consts.start,consts.start,    4,consts.error, // 10-17
        5,consts.error,    6,consts.error,    5,    5,    4,consts.error, // 18-1f
        4,consts.error,    4,    4,    4,consts.error,    4,consts.error, // 20-27
        4,consts.itsMe,consts.start,consts.start,consts.start,consts.start,consts.start,consts.start  // 28-2f
];

jschardet.HZCharLenTable = [0, 0, 0, 0, 0, 0];

jschardet.HZSMModel = {
    "classTable"    : jschardet.HZ_cls,
    "classFactor"   : 6,
    "stateTable"    : jschardet.HZ_st,
    "charLenTable"  : jschardet.HZCharLenTable,
    "name"          : "HZ-GB-2312"
};

jschardet.ISO2022CN_cls = [
    2,0,0,0,0,0,0,0,  // 00 - 07
    0,0,0,0,0,0,0,0,  // 08 - 0f
    0,0,0,0,0,0,0,0,  // 10 - 17
    0,0,0,1,0,0,0,0,  // 18 - 1f
    0,0,0,0,0,0,0,0,  // 20 - 27
    0,3,0,0,0,0,0,0,  // 28 - 2f
    0,0,0,0,0,0,0,0,  // 30 - 37
    0,0,0,0,0,0,0,0,  // 38 - 3f
    0,0,0,4,0,0,0,0,  // 40 - 47
    0,0,0,0,0,0,0,0,  // 48 - 4f
    0,0,0,0,0,0,0,0,  // 50 - 57
    0,0,0,0,0,0,0,0,  // 58 - 5f
    0,0,0,0,0,0,0,0,  // 60 - 67
    0,0,0,0,0,0,0,0,  // 68 - 6f
    0,0,0,0,0,0,0,0,  // 70 - 77
    0,0,0,0,0,0,0,0,  // 78 - 7f
    2,2,2,2,2,2,2,2,  // 80 - 87
    2,2,2,2,2,2,2,2,  // 88 - 8f
    2,2,2,2,2,2,2,2,  // 90 - 97
    2,2,2,2,2,2,2,2,  // 98 - 9f
    2,2,2,2,2,2,2,2,  // a0 - a7
    2,2,2,2,2,2,2,2,  // a8 - af
    2,2,2,2,2,2,2,2,  // b0 - b7
    2,2,2,2,2,2,2,2,  // b8 - bf
    2,2,2,2,2,2,2,2,  // c0 - c7
    2,2,2,2,2,2,2,2,  // c8 - cf
    2,2,2,2,2,2,2,2,  // d0 - d7
    2,2,2,2,2,2,2,2,  // d8 - df
    2,2,2,2,2,2,2,2,  // e0 - e7
    2,2,2,2,2,2,2,2,  // e8 - ef
    2,2,2,2,2,2,2,2,  // f0 - f7
    2,2,2,2,2,2,2,2   // f8 - ff
];

jschardet.ISO2022CN_st = [
    consts.start,    3,consts.error,consts.start,consts.start,consts.start,consts.start,consts.start, // 00-07
    consts.start,consts.error,consts.error,consts.error,consts.error,consts.error,consts.error,consts.error, // 08-0f
    consts.error,consts.error,consts.itsMe,consts.itsMe,consts.itsMe,consts.itsMe,consts.itsMe,consts.itsMe, // 10-17
    consts.itsMe,consts.itsMe,consts.itsMe,consts.error,consts.error,consts.error,    4,consts.error, // 18-1f
    consts.error,consts.error,consts.error,consts.itsMe,consts.error,consts.error,consts.error,consts.error, // 20-27
        5,    6,consts.error,consts.error,consts.error,consts.error,consts.error,consts.error, // 28-2f
    consts.error,consts.error,consts.error,consts.itsMe,consts.error,consts.error,consts.error,consts.error, // 30-37
    consts.error,consts.error,consts.error,consts.error,consts.error,consts.itsMe,consts.error,consts.start  // 38-3f
];

jschardet.ISO2022CNCharLenTable = [0, 0, 0, 0, 0, 0, 0, 0, 0];

jschardet.ISO2022CNSMModel = {
    "classTable"    : jschardet.ISO2022CN_cls,
    "classFactor"   : 9,
    "stateTable"    : jschardet.ISO2022CN_st,
    "charLenTable"  : jschardet.ISO2022CNCharLenTable,
    "name"          : "ISO-2022-CN"
};

jschardet.ISO2022JP_cls = [
    2,0,0,0,0,0,0,0,  // 00 - 07
    0,0,0,0,0,0,2,2,  // 08 - 0f
    0,0,0,0,0,0,0,0,  // 10 - 17
    0,0,0,1,0,0,0,0,  // 18 - 1f
    0,0,0,0,7,0,0,0,  // 20 - 27
    3,0,0,0,0,0,0,0,  // 28 - 2f
    0,0,0,0,0,0,0,0,  // 30 - 37
    0,0,0,0,0,0,0,0,  // 38 - 3f
    6,0,4,0,8,0,0,0,  // 40 - 47
    0,9,5,0,0,0,0,0,  // 48 - 4f
    0,0,0,0,0,0,0,0,  // 50 - 57
    0,0,0,0,0,0,0,0,  // 58 - 5f
    0,0,0,0,0,0,0,0,  // 60 - 67
    0,0,0,0,0,0,0,0,  // 68 - 6f
    0,0,0,0,0,0,0,0,  // 70 - 77
    0,0,0,0,0,0,0,0,  // 78 - 7f
    2,2,2,2,2,2,2,2,  // 80 - 87
    2,2,2,2,2,2,2,2,  // 88 - 8f
    2,2,2,2,2,2,2,2,  // 90 - 97
    2,2,2,2,2,2,2,2,  // 98 - 9f
    2,2,2,2,2,2,2,2,  // a0 - a7
    2,2,2,2,2,2,2,2,  // a8 - af
    2,2,2,2,2,2,2,2,  // b0 - b7
    2,2,2,2,2,2,2,2,  // b8 - bf
    2,2,2,2,2,2,2,2,  // c0 - c7
    2,2,2,2,2,2,2,2,  // c8 - cf
    2,2,2,2,2,2,2,2,  // d0 - d7
    2,2,2,2,2,2,2,2,  // d8 - df
    2,2,2,2,2,2,2,2,  // e0 - e7
    2,2,2,2,2,2,2,2,  // e8 - ef
    2,2,2,2,2,2,2,2,  // f0 - f7
    2,2,2,2,2,2,2,2   // f8 - ff
];

jschardet.ISO2022JP_st = [
    consts.start,    3,consts.error,consts.start,consts.start,consts.start,consts.start,consts.start, // 00-07
    consts.start,consts.start,consts.error,consts.error,consts.error,consts.error,consts.error,consts.error, // 08-0f
    consts.error,consts.error,consts.error,consts.error,consts.itsMe,consts.itsMe,consts.itsMe,consts.itsMe, // 10-17
    consts.itsMe,consts.itsMe,consts.itsMe,consts.itsMe,consts.itsMe,consts.itsMe,consts.error,consts.error, // 18-1f
    consts.error,    5,consts.error,consts.error,consts.error,    4,consts.error,consts.error, // 20-27
    consts.error,consts.error,consts.error,    6,consts.itsMe,consts.error,consts.itsMe,consts.error, // 28-2f
    consts.error,consts.error,consts.error,consts.error,consts.error,consts.error,consts.itsMe,consts.itsMe, // 30-37
    consts.error,consts.error,consts.error,consts.itsMe,consts.error,consts.error,consts.error,consts.error, // 38-3f
    consts.error,consts.error,consts.error,consts.error,consts.itsMe,consts.error,consts.start,consts.start  // 40-47
];

jschardet.ISO2022JPCharLenTable = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0];

jschardet.ISO2022JPSMModel = {
    "classTable"    : jschardet.ISO2022JP_cls,
    "classFactor"   : 10,
    "stateTable"    : jschardet.ISO2022JP_st,
    "charLenTable"  : jschardet.ISO2022JPCharLenTable,
    "name"          : "ISO-2022-JP"
};

jschardet.ISO2022KR_cls = [
    2,0,0,0,0,0,0,0,  // 00 - 07
    0,0,0,0,0,0,0,0,  // 08 - 0f
    0,0,0,0,0,0,0,0,  // 10 - 17
    0,0,0,1,0,0,0,0,  // 18 - 1f
    0,0,0,0,3,0,0,0,  // 20 - 27
    0,4,0,0,0,0,0,0,  // 28 - 2f
    0,0,0,0,0,0,0,0,  // 30 - 37
    0,0,0,0,0,0,0,0,  // 38 - 3f
    0,0,0,5,0,0,0,0,  // 40 - 47
    0,0,0,0,0,0,0,0,  // 48 - 4f
    0,0,0,0,0,0,0,0,  // 50 - 57
    0,0,0,0,0,0,0,0,  // 58 - 5f
    0,0,0,0,0,0,0,0,  // 60 - 67
    0,0,0,0,0,0,0,0,  // 68 - 6f
    0,0,0,0,0,0,0,0,  // 70 - 77
    0,0,0,0,0,0,0,0,  // 78 - 7f
    2,2,2,2,2,2,2,2,  // 80 - 87
    2,2,2,2,2,2,2,2,  // 88 - 8f
    2,2,2,2,2,2,2,2,  // 90 - 97
    2,2,2,2,2,2,2,2,  // 98 - 9f
    2,2,2,2,2,2,2,2,  // a0 - a7
    2,2,2,2,2,2,2,2,  // a8 - af
    2,2,2,2,2,2,2,2,  // b0 - b7
    2,2,2,2,2,2,2,2,  // b8 - bf
    2,2,2,2,2,2,2,2,  // c0 - c7
    2,2,2,2,2,2,2,2,  // c8 - cf
    2,2,2,2,2,2,2,2,  // d0 - d7
    2,2,2,2,2,2,2,2,  // d8 - df
    2,2,2,2,2,2,2,2,  // e0 - e7
    2,2,2,2,2,2,2,2,  // e8 - ef
    2,2,2,2,2,2,2,2,  // f0 - f7
    2,2,2,2,2,2,2,2   // f8 - ff
];

jschardet.ISO2022KR_st = [
    consts.start,    3,consts.error,consts.start,consts.start,consts.start,consts.error,consts.error, // 00-07
    consts.error,consts.error,consts.error,consts.error,consts.itsMe,consts.itsMe,consts.itsMe,consts.itsMe, // 08-0f
    consts.itsMe,consts.itsMe,consts.error,consts.error,consts.error,    4,consts.error,consts.error, // 10-17
    consts.error,consts.error,consts.error,consts.error,    5,consts.error,consts.error,consts.error, // 18-1f
    consts.error,consts.error,consts.error,consts.itsMe,consts.start,consts.start,consts.start,consts.start  // 20-27
];

jschardet.ISO2022KRCharLenTable = [0, 0, 0, 0, 0, 0];

jschardet.ISO2022KRSMModel = {
    "classTable"    : jschardet.ISO2022KR_cls,
    "classFactor"   : 6,
    "stateTable"    : jschardet.ISO2022KR_st,
    "charLenTable"  : jschardet.ISO2022KRCharLenTable,
    "name"          : "ISO-2022-KR"
};

}(require('./init'));
