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
"Tests that ArrayPop is known to the DFG to be a side effect."
);

function foo(a, b) {
    var result = a.f;
    result += b.pop();
    result += a.g;
    return result;
}

var ouches = 0;
for (var i = 0; i < 200; ++i) {
    var a = {f:1, g:2};
    var b = [];
    var expected;
    if (i < 150) {
        // Ensure that we always transition the array's structure to one that indicates
        // that we have array storage.
        b.__defineGetter__("0", function() {
            testFailed("Should never get here");
        });
        b.length = 0;
        b[0] = 42;
        expected = "45";
    } else {
        b.__defineGetter__("0", function() {
            debug("Ouch!");
            ouches++;
            delete a.g;
            a.h = 43;
            return 5;
        });
        expected = "0/0";
    }
    shouldBe("foo(a, b)", expected);
}

shouldBe("ouches", "50");
