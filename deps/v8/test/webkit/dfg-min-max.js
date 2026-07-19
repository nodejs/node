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
"This tests that Math.min and Math.max for doubles works correctly in the DFG JIT."
);

function doMin(a, b) {
    return Math.min(a, b);
}

function doMax(a, b) {
    return Math.max(a, b);
}

for (var i = 0; i < 1000; ++i) {
    doMin(1.5, 2.5);
    doMax(1.5, 2.5);
}

shouldBe("doMin(1.5, 2.5)", "1.5");
shouldBe("doMin(2.5, 1.5)", "1.5");
shouldBe("doMin(1.5, 1.5)", "1.5");
shouldBe("doMin(2.5, 2.5)", "2.5");

shouldBe("doMin(1.5, NaN)", "NaN");
shouldBe("doMin(2.5, NaN)", "NaN");
shouldBe("doMin(NaN, 1.5)", "NaN");
shouldBe("doMin(NaN, 2.5)", "NaN");

shouldBe("doMin(NaN, NaN)", "NaN");

shouldBe("doMax(1.5, 2.5)", "2.5");
shouldBe("doMax(2.5, 1.5)", "2.5");
shouldBe("doMax(1.5, 1.5)", "1.5");
shouldBe("doMax(2.5, 2.5)", "2.5");

shouldBe("doMax(1.5, NaN)", "NaN");
shouldBe("doMax(2.5, NaN)", "NaN");
shouldBe("doMax(NaN, 1.5)", "NaN");
shouldBe("doMax(NaN, 2.5)", "NaN");

shouldBe("doMax(NaN, NaN)", "NaN");

var successfullyParsed = true;
