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
'This is a test of the charAt and charCodeAt string functions.'
);

var undefined;

var cases = [
    ["", "omitted"],
    ["", undefined],
    ["", 0],
    ["", null],
    ["", false],
    ["", true],
    ["", 0.0],
    ["", 0.1],
    ["", 999],
    ["", 1/0],
    ["", -1],
    ["", -1/0],
    ["", 0/0],

    ["x", "omitted"],
    ["x", undefined],
    ["x", 0],
    ["x", null],
    ["x", false],
    ["x", true],
    ["x", 0.0],
    ["x", 0.1],
    ["x", 999],
    ["x", 1/0],
    ["x", -1],
    ["x", -1/0],
    ["x", 0/0],

    ["xy", "omitted"],
    ["xy", undefined],
    ["xy", 0],
    ["xy", null],
    ["xy", false],
    ["xy", true],
    ["xy", 0.0],
    ["xy", 0.1],
    ["xy", 999],
    ["xy", 1/0],
    ["xy", -1],
    ["xy", -1/0],
    ["xy", 0/0],
];

var answers = [['""', 'NaN'],
['""', 'NaN'],
['""', 'NaN'],
['""', 'NaN'],
['""', 'NaN'],
['""', 'NaN'],
['""', 'NaN'],
['""', 'NaN'],
['""', 'NaN'],
['""', 'NaN'],
['""', 'NaN'],
['""', 'NaN'],
['""', 'NaN'],
['"x"', '120'],
['"x"', '120'],
['"x"', '120'],
['"x"', '120'],
['"x"', '120'],
['""', 'NaN'],
['"x"', '120'],
['"x"', '120'],
['""', 'NaN'],
['""', 'NaN'],
['""', 'NaN'],
['""', 'NaN'],
['"x"', '120'],
['"x"', '120'],
['"x"', '120'],
['"x"', '120'],
['"x"', '120'],
['"x"', '120'],
['"y"', '121'],
['"x"', '120'],
['"x"', '120'],
['""', 'NaN'],
['""', 'NaN'],
['""', 'NaN'],
['""', 'NaN'],
['"x"', '120']];

for (var i = 0; i < cases.length; ++i)
{
    var item = cases[i];
    var result = answers[i];
    if (item[1] == "omitted") {
        shouldBe('"' + item[0] + '".charAt()', result[0]);
        if (result[1] == 'NaN')
            shouldBeNaN('"' + item[0] + '".charCodeAt()');
        else
            shouldBe('"' + item[0] + '".charCodeAt()', result[1]);
    } else {
        shouldBe('"' + item[0] + '".charAt(' + item[1] + ')', result[0]);
        if (result[1] == 'NaN')
            shouldBeNaN('"' + item[0] + '".charCodeAt(' + item[1] + ')');
        else
            shouldBe('"' + item[0] + '".charCodeAt(' + item[1] + ')', result[1]);
    }
}
