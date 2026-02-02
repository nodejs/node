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
"Tests that when values predicted but not proven int are used in a tower of additions, we don't eliminate the overflow check unsoundly."
);

function foo(a, b, o) {
    return (a + b + o.f) | 0;
}

function bar(a, b, o) {
    eval(""); // Prevent this function from being opt compiled.
    return foo(a, b, o);
}

var badCases = [
    {a:2147483645, b:2147483644, c:9007199254740990, expected:-8},
    {a:2147483643, b:2147483643, c:18014398509481980, expected:-16},
    {a:2147483643, b:2147483642, c:36028797018963960, expected:-16},
    {a:2147483642, b:2147483642, c:36028797018963960, expected:-16},
    {a:2147483641, b:2147483640, c:144115188075855840, expected:-32},
    {a:2147483640, b:2147483640, c:144115188075855840, expected:-64},
    {a:2147483640, b:2147483639, c:288230376151711680, expected:-64},
    {a:2147483639, b:2147483639, c:288230376151711680, expected:-64}
];

var warmup = 100;

for (var i = 0; i < warmup + badCases.length; ++i) {
    var a, b, c;
    var expected;
    if (i < warmup) {
        a = 1;
        b = 2;
        c = 3;
        expected = 6;
    } else {
        var current = badCases[i - warmup];
        a = current.a;
        b = current.b;
        c = current.c;
        expected = current.expected;
    }
    shouldBe("bar(" + a + ", " + b + ", {f:" + c + "})", "" + expected);
}
