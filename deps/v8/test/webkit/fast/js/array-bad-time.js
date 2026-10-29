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
"Tests that defining a setter on the Array prototype works even if it is done after arrays are allocated."
);

var ouches = 0;

function foo(haveABadTime) {
    var result = [];
    result.length = 5;
    for (var i = 0; i < result.length; ++i) {
        if (i == haveABadTime) {
            debug("Henceforth I will have a bad time.");
            Array.prototype.__defineSetter__("3", function() { ouches++; });
        }
        result[i] = i;
    }
    return result;
}

var expected = "\"0,1,2,3,4\"";

for (var i = 0; i < 1000; ++i) {
    var haveABadTime;
    if (i == 950) {
        haveABadTime = 2;
        expected = "\"0,1,2,,4\"";
    } else
        haveABadTime = -1;
    shouldBe("\"" + foo(haveABadTime).join(",") + "\"", expected);
}

shouldBe("ouches", "50");
