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
"This test checks that n % 0 doesn't crash with a floating-point exception."
);

shouldBe("2 % 0", "NaN");

var n = 2;
shouldBe("n % 0", "NaN");

function f()
{
    return 2 % 0;
}

shouldBe("f()", "NaN");

function g()
{
    var n = 2;
    return n % 0;
}

shouldBe("g()", "NaN");

// Test that reusing a floating point value after use in a modulus works correctly.
function nonSpeculativeModReuseInner(argument, o1, o2)
{
         // The + operator on objects is a reliable way to avoid the speculative JIT path for now at least.
    o1 + o2;

    var knownDouble = argument - 0;
    return knownDouble % 1 + knownDouble;
}
function nonSpeculativeModReuse(argument)
{
    return nonSpeculativeModReuseInner(argument, {}, {});
}

shouldBe("nonSpeculativeModReuse(0.5)", "1");
