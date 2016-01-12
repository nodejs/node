//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

Object.defineProperty(Object.getPrototypeOf({}), "echo", { value: function () { WScript.Echo(this); } });
Object.defineProperty(Object.getPrototypeOf({}), "echos", { value: function () { WScript.Echo(JSON.stringify(this)); } });
function AssertEqual(actual, expected, msg) { ((actual === expected ? "Passed! " : "Failed (actual: " + actual + ", expected: " + expected + "). Message: ") + msg).echo(); };
Object.defineProperty(Object.getPrototypeOf({}), "equalTo", { value: function (other, msg) { AssertEqual(this.constructor(this), other, msg); } });

//  Range:                      Bit Mask:
//  0x000000 - 0x00007F         0xxxxxxx                                        0 - 128
//  0x000080 - 0x0007FF         110xxxxx    10xxxxxx                            49280 - 65535
//  0x000800 - 0x00FFFF         1110xxxx    10xxxxxx    10xxxxxx                14712960 - 33554431
//  0x010000 - 0x1FFFFF         11110xxx    10xxxxxx    10xxxxxx    10xxxxxx    -260013952 - -1


var strArr = Array(400000);
for (var j = 0; j < 100000; j++) {
    var val = Math.floor(Math.random() * 0x7F);
    strArr[j] = String.fromCodePoint(val);
}
for (; j < 200000; j++) {
    var val = Math.floor(Math.random() * 0x77F) + 0x80;
    strArr[j] = String.fromCodePoint(val);
}
for (; j < 300000; j++) {
    var val = Math.floor(Math.random() * 0xF7FF) + 0x800;
    strArr[j] = String.fromCodePoint(val);
}
for (; j < 400000; j++) {
    var val = Math.floor(Math.random() * 0x0FFFFF) + 0x010000;
    strArr[j] = String.fromCodePoint(val);
}

var str = strArr.join('');
var output = "";
var utf8 = [];
var i = 0;
var start = Date.now();
while (i < str.length) {
    var codePoint = str.codePointAt(i);
    utf8.push(convertUTF16ValueToUTF8(codePoint));
    i += (codePoint >= 0x10000 ? 2 : 1);
}


for (var i = 0; i < utf8.length; i++) {
    var item = utf8[i];
    var codePoint = convertUTF8ValueToUTF16(item);
    if (String.fromCodePoint(codePoint).length > 2) codePoint.echo();
    output += String.fromCodePoint(codePoint);
}


var total = Date.now() - start;

if (output === str) {
//    total.echo();
    "Pass".echo();
} else {
    "Failed!".echo();
}

function convertUTF16ValueToUTF8(value) {
    if (value < 0x80) return value;
    if (value < 0x800) return (((value >> 6) + 0xC0 /* 11000000 */) << 8) + (value & 0x3F) + 0x80;
    if (value < 0x10000) return (((value >> 12) + 0xE0 /* 11100000 */) << 16) + ((((value >> 6) & 0x3F) + 0x80) << 8) + (value & 0x3F) + 0x80;
    if (value < 0x200000) return (((value >> 18) + 0xF0 /* 11100000 */) << 24) + ((((value >> 12) & 0x3F) + 0x80) << 16) + ((((value >> 6) & 0x3F) + 0x80) << 8) + (value & 0x3F) + 0x80;
}

function convertUTF8ValueToUTF16(value) {
    if (value <= -1 && value >= -260013952) return (((value >> 24) & 0x7) << 18) + (((value >> 16) & 0x3F) << 12) + (((value >> 8) & 0x3F) << 6) + (value & 0x3F);
    if (value <= 127 && value >= 0) return value;
    if (value <= 65535 && value >= 49280) return (((value >> 8) & 0x1F) << 6) + (value & 0x3F);
    if (value <= 33554431 && value >= 14712960) return (((value >> 16) & 0xF) << 12) + (((value >> 8) & 0x3F) << 6) + (value & 0x3F);

    throw new Error(value);
}