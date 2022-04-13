// Copyright 2013 the V8 project authors. All rights reserved.
// Copyright (C) 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1.  Redistributions of source code must retain the above copyright
//     notice, this list of conditions and the following disclaimer.
// 2.  Redistributions in binary form must reproduce the above copyright
//     notice, this list of conditions and the following disclaimer in the
//     documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
// ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

description(
'This test case tests the Date constructor. ' +
'In particular, it tests many cases of creating a Date from another Date ' +
'and creating a Date from an object that has both valueOf and toString functions.'
);

var object = new Object;
object.valueOf = function() { return 1995; }
object.toString = function() { return "2222"; }

shouldBe('isNaN(new Date(""))', 'true');

var timeZoneOffset = Date.parse("Feb 1 1995") - Date.parse("Feb 1 1995 GMT");

shouldBe('new Date(1995).getTime()', '1995');
shouldBe('new Date(object).getTime()', '1995');
shouldBe('new Date(new Date(1995)).getTime()', '1995');
shouldBe('new Date(new Date(1995).toString()).getTime()', '1000');

shouldBe('new Date(1995, 1).getTime() - timeZoneOffset', '791596800000');
shouldBe('new Date(1995, 1, 1).getTime() - timeZoneOffset', '791596800000');
shouldBe('new Date(1995, 1, 1, 1).getTime() - timeZoneOffset', '791600400000');
shouldBe('new Date(1995, 1, 1, 1, 1).getTime() - timeZoneOffset', '791600460000');
shouldBe('new Date(1995, 1, 1, 1, 1, 1).getTime() - timeZoneOffset', '791600461000');
shouldBe('new Date(1995, 1, 1, 1, 1, 1, 1).getTime() - timeZoneOffset', '791600461001');
shouldBe('new Date(1995, 1, 1, 1, 1, 1, 1, 1).getTime() - timeZoneOffset', '791600461001');
shouldBe('new Date(1995, 1, 1, 1, 1, 1, 1, 1, 1).getTime() - timeZoneOffset', '791600461001');

shouldBe('new Date(new Date(1995, 1)).getTime() - timeZoneOffset', '791596800000');
shouldBe('new Date(new Date(1995, 1, 1)).getTime() - timeZoneOffset', '791596800000');
shouldBe('new Date(new Date(1995, 1, 1, 1)).getTime() - timeZoneOffset', '791600400000');
shouldBe('new Date(new Date(1995, 1, 1, 1, 1)).getTime() - timeZoneOffset', '791600460000');
shouldBe('new Date(new Date(1995, 1, 1, 1, 1, 1)).getTime() - timeZoneOffset', '791600461000');
shouldBe('new Date(new Date(1995, 1, 1, 1, 1, 1, 1)).getTime() - timeZoneOffset', '791600461001');
shouldBe('new Date(new Date(1995, 1, 1, 1, 1, 1, 1, 1)).getTime() - timeZoneOffset', '791600461001');
shouldBe('new Date(new Date(1995, 1, 1, 1, 1, 1, 1, 1, 1)).getTime() - timeZoneOffset', '791600461001');

shouldBe("Number(new Date(new Date(Infinity, 1, 1, 1, 1, 1, 1, 1, 1)).getTime() - timeZoneOffset)", 'Number.NaN');
shouldBe("Number(new Date(new Date(95, Infinity, 1, 1, 1, 1, 1, 1, 1)).getTime() - timeZoneOffset)", 'Number.NaN');
shouldBe("Number(new Date(new Date(95, 1, Infinity, 1, 1, 1, 1, 1, 1)).getTime() - timeZoneOffset)", 'Number.NaN');
shouldBe("Number(new Date(new Date(95, 1, 1, Infinity, 1, 1, 1, 1, 1)).getTime() - timeZoneOffset)", 'Number.NaN');
shouldBe("Number(new Date(new Date(95, 1, 1, 1, Infinity, 1, 1, 1, 1)).getTime() - timeZoneOffset)", 'Number.NaN');
shouldBe("Number(new Date(new Date(95, 1, 1, 1, 1, Infinity, 1, 1, 1)).getTime() - timeZoneOffset)", 'Number.NaN');
shouldBe("Number(new Date(new Date(95, 1, 1, 1, 1, 1, Infinity, 1, 1)).getTime() - timeZoneOffset)", 'Number.NaN');
shouldBe("Number(new Date(new Date(95, 1, 1, 1, 1, 1, 1, 1, Infinity)).getTime() - timeZoneOffset)", '791600461001');

// In Firefox, the results of the following tests are timezone-dependent, which likely implies that the implementation is not quite correct.
// Our results are even worse, though, as the dates are clipped: (new Date(1111, 1201).getTime()) == (new Date(1111, 601).getTime())
// shouldBe('new Date(1111, 1111, 1111, 1111, 1111, 1111, 1111, 1111).getTime() - timeZoneOffset', '-24085894227889');
// shouldBe('new Date(new Date(1111, 1111, 1111, 1111, 1111, 1111, 1111, 1111)).getTime() - timeZoneOffset', '-24085894227889');

// Regression test for Bug 26978 (https://bugs.webkit.org/show_bug.cgi?id=26978)
var testStr = "";
var year = { valueOf: function() { testStr += 1; return 2007; } };
var month = { valueOf: function() { testStr += 2; return 2; } };
var date = { valueOf: function() { testStr += 3; return 4; } };
var hours = { valueOf: function() { testStr += 4; return 13; } };
var minutes = { valueOf: function() { testStr += 5; return 50; } };
var seconds = { valueOf: function() { testStr += 6; return 0; } };
var ms = { valueOf: function() { testStr += 7; return 999; } };

testStr = "";
new Date(year, month, date, hours, minutes, seconds, ms);
shouldBe('testStr', '\"1234567\"');

testStr = "";
Date.UTC(year, month, date, hours, minutes, seconds, ms);
shouldBe('testStr', '\"1234567\"');
