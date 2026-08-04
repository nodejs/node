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

﻿description(
"Bug 7445, bug 7253: Handle Unicode escapes in regexps."
);

var I3=/^\s*(fwd|re|aw|antw|antwort|wg|sv|ang|odp|betreff|betr|transf|reenv\.|reenv|in|res|resp|resp\.|enc|\u8f6c\u53d1|\u56DE\u590D|\u041F\u0435\u0440\u0435\u0441\u043B|\u041E\u0442\u0432\u0435\u0442):\s*(.*)$/i;

// Other RegExs from Gmail source
var Ci=/\s+/g;
var BC=/^ /;
var BG=/ $/;

// Strips leading Re or similar (from Gmail source)
function cy(a) {
    //var b = I3.exec(a);
    var b = I3.exec(a);

    if (b) {
        a = b[2];
    }

    return Gn(a);
}

// This function replaces consecutive whitespace with a single space
// then removes a leading and trailing space if they exist. (From Gmail)
function Gn(a) {
    return a.replace(Ci, " ").replace(BC, "").replace(BG, "");
}

shouldBe('cy("Re: Hello")', '"Hello"');
shouldBe('cy("Ответ: Hello")', '"Hello"');

// ---------------------------------------------------------------

var regex = /^([^#<\u2264]+)([#<\u2264])(.*)$/;
shouldBe('regex.exec("24#Midnight").toString()', '"24#Midnight,24,#,Midnight"');
