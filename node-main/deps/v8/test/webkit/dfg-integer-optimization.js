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
"This tests that integer addition optimizations in the DFG are not performed too overzealously."
);

function doAdd(a,b) {
    // The point of this test is to see if the DFG CSE's the second (a + b) against the first, after
    // optimizing the first to be an integer addition. The first one certainly is an integer addition,
    // but the second one isn't - it must either be an integer addition with overflow checking, or a
    // double addition.
    return {a:((a + b) | 0), b:(a + b)};
}

for (var i = 0; i < 1000; ++i) {
    // Create numbers big enough that we'll start seeing doubles only after about 200 invocations.
    var a = i * 1000 * 1000 * 10;
    var b = i * 1000 * 1000 * 10 + 1;
    var result = doAdd(a, b);

    // Use eval() for computing the correct result, to force execution to happen outside the DFG.
    shouldBe("result.a", "" + eval("((" + a + " + " + b + ") | 0)"))
    shouldBe("result.b", "" + eval(a + " + " + b))
}
