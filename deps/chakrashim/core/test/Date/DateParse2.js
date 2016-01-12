//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

﻿myPrint("Phase 1 - known dates");

myPrint("A --");
testDate(new Date(-2012, 1, 2, 1, 2, 3));
testParseDate(new Date(-2012, 1, 2, 1, 2, 3).toString());
testParseDate(new Date(-2012, 1, 2, 1, 2, 3).toUTCString());
testParseDate(new Date(-2012, 1, 2, 1, 2, 3).toISOString());

myPrint("B --");
testDate(new Date(2012, 1, 2, 1, 2, 3));
testParseDate(new Date(2012, 1, 2, 1, 2, 3).toString());
testParseDate(new Date(2012, 1, 2, 1, 2, 3).toUTCString());
testParseDate(new Date(2012, 1, 2, 1, 2, 3).toISOString());

myPrint("C --");
testDate(new Date(99999, 1, 2, 1, 2, 3));
testParseDate(new Date(99999, 1, 2, 1, 2, 3).toString());
testParseDate(new Date(99999, 1, 2, 1, 2, 3).toUTCString());
testParseDate(new Date(99999, 1, 2, 1, 2, 3).toISOString());

myPrint("D --");
testDate(new Date(-99999, 1, 2, 1, 2, 3));
testParseDate(new Date(-99999, 1, 2, 1, 2, 3).toString());
testParseDate(new Date(-99999, 1, 2, 1, 2, 3).toUTCString());
testParseDate(new Date(-99999, 1, 2, 1, 2, 3).toISOString());

myPrint("E --");
testDate(new Date(-12, 1, 2, 1, 2, 3));
testParseDate(new Date(-12, 1, 2, 1, 2, 3).toString());
testParseDate(new Date(-12, 1, 2, 1, 2, 3).toUTCString());
testParseDate(new Date(-12, 1, 2, 1, 2, 3).toISOString());

myPrint("F --");
testDate(new Date(12, 1, 2, 1, 2, 3));
testParseDate(new Date(12, 1, 2, 1, 2, 3).toString());
testParseDate(new Date(12, 1, 2, 1, 2, 3).toUTCString());
testParseDate(new Date(12, 1, 2, 1, 2, 3).toISOString());

myPrint("Phase 2 - parsing sample date strings");
testParseDate("Tue Feb 02 2012 01:02:03 GMT-0800 (Pacific Standard Time)");
testParseDate("Tue Feb 02 2012 01:02:03 GMT+0800 (prisec)");
testParseDate2("Tue Feb 02 2012 01:02:03 GMT+0000", " (ﾊﾇ)");
testParseDate("Tue Feb 02 2012 01:02:03 GMT-0000");
testParseDate("Tue Feb 02 2012 01:02:03 GMT+0430 (prisec@)");
testParseDate("Tue Feb 2 01:02:03 PST 2013 B.C.");
testParseDate("Thu Feb 2 01:02:03 PST 2012");

function testDate(date) {
    testParseDate(date.toString());
}

function testParseDate(dateStr) {
    myPrint("Date string:\t\t" + dateStr);
    var d = Date.parse(dateStr);
    testParseDateCore(d);
}

// This is to avoid printing non-printable chars
function testParseDate2(dateStr, appendThis) {
    myPrint("Date string:\t\t" + dateStr);
    var d = Date.parse(dateStr + appendThis);
    testParseDateCore(d);
}

function testParseDateCore(d) {
    myPrint("\t raw:\t\t" + d);
    d = new Date(d);
    myPrint("\t toString:\t" + d.toString());
    myPrint("\t toUTCString:\t" + d.toUTCString());
    myPrint("\t toGMTString:\t" + d.toGMTString());
    if (isNaN(d) === false) {
        myPrint("\t toISOString:\t" + d.toISOString());
        myPrint("\t\t\t" + d.getDate() + " " + d.getTime() + " " + d.getTimezoneOffset());
        myPrint("\t\t\t" + d.getFullYear() + "/" + d.getMonth() + "/" + d.getDay());
        myPrint("\t\t\t" + d.getHours() + ":" + d.getMinutes() + ":" + d.getSeconds() + "." + d.getMilliseconds());
    }
    myPrint("");
}

function myPrint(str) {
    if (WScript.Echo !== undefined) {
        WScript.Echo(str);
    }
    else {
        throw "no print!";
    }
}
