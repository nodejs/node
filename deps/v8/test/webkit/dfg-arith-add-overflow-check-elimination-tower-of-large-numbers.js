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
"Tests that if we have a tower of large numerical constants being added to each other, the DFG knows that a sufficiently large tower may produce a large enough value that overflow check elimination must be careful."
);

function foo(a, b) {
    return (a + b + 281474976710655 + 281474976710655 + 281474976710655 + 281474976710655 +
            281474976710655 + 281474976710655 + 281474976710655 + 281474976710655 +
            281474976710655 + 281474976710655 + 281474976710655 + 281474976710655 +
            281474976710655 + 281474976710655 + 281474976710655 + 281474976710655 +
            281474976710655 + 281474976710655 + 281474976710655 + 281474976710655 +
            281474976710655 + 281474976710655 + 281474976710655 + 281474976710655 +
            281474976710655 + 281474976710655 + 281474976710655 + 281474976710655 +
            281474976710655 + 281474976710655 + 281474976710655 + 281474976710655 + 30) | 0;
}

function bar(a, b, o) {
    eval(""); // Prevent this function from being opt compiled.
    return foo(a, b, o);
}

var warmup = 200;

for (var i = 0; i < warmup + 1; ++i) {
    var a, b, c;
    var expected;
    if (i < warmup) {
        a = 1;
        b = 2;
        expected = 0;
    } else {
        a = 2147483645;
        b = 2147483644;
        expected = -10;
    }
    shouldBe("bar(" + a + ", " + b + ")", "" + expected);
}
