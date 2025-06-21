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

// 15.4 Array Objects
// (c) 2001 Harri Porten <porten@kde.org>

description(
'This test checks for regression against <a href="https://bugs.webkit.org/show_bug.cgi?id=4147">4147: Array.toString() and toLocaleString() improvements from KDE KJS</a>.'
);

// backup
var backupNumberToString = Number.prototype.toString;
var backupNumberToLocaleString = Number.prototype.toLocaleString;
var backupRegExpToString = RegExp.prototype.toString;
var backupRegExpToLocaleString = RegExp.prototype.toLocaleString;

// change functions
Number.prototype.toString = function() { return "toString"; }
Number.prototype.toLocaleString = function() { return "toLocaleString"; }
RegExp.prototype.toString = function() { return "toString2"; }
RegExp.prototype.toLocaleString = function() { return "toLocaleString2"; }

// the tests
shouldBe("[1].toString()", "'1'");
shouldBe("[1].toLocaleString()", "'toLocaleString'");
Number.prototype.toLocaleString = "invalid";
shouldBe("[1].toLocaleString()", "'1'");
shouldBe("[/r/].toString()", "'toString2'");
shouldBe("[/r/].toLocaleString()", "'toLocaleString2'");
RegExp.prototype.toLocaleString = "invalid";
shouldBe("[/r/].toLocaleString()", "'toString2'");

var caught = false;
try {
[{ toString : 0 }].toString();
} catch (e) {
caught = true;
}
shouldBeTrue("caught");

// restore
Number.prototype.toString = backupNumberToString;
Number.prototype.toLocaleString = backupNumberToLocaleString;
RegExp.prototype.toString = backupRegExpToString;
RegExp.prototype.toLocaleString = backupRegExpToLocaleString;
