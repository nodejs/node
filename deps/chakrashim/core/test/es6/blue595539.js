//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function testES6Whitespace(whitespaceChar, whitespaceCode) {
    try {
        var str = "var " + whitespaceChar + "a = 5;";
        eval(str);
        if (a !== 5) {
            throw new Error("Eval value didn't equal to 5.");
        }
    } catch (ex) {
        WScript.Echo("Whitespace error with: " + whitespaceCode + "\r\nMessage: " + ex.message);
    }
}

var whitespaceChars = [
    { code: 0x9, strValue: "0x9" },
    { code: 0xB, strValue: "0xB" },
    { code: 0xC, strValue: "0xC" },
    { code: 0x20, strValue: "0x20" },
    { code: 0xA0, strValue: "0xA0" },
    { code: 0xFEFF, strValue: "0xFEFF" }];

whitespaceChars.forEach(function (item) { testES6Whitespace(String.fromCharCode(item.code), item.strValue); });

WScript.Echo("Pass");