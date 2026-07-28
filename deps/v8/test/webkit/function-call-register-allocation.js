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

// This neuters too low stack size passed by the flag fuzzer.
// Flags: --stack-size=864

description(
"This test checks for a specific regression that caused function calls to allocate too many temporary registers."
);

var message = "PASS: Recursion did not run out of stack space."
try {
    // Call a function recursively.
    (function f(g, x) {
        if (x > 3000)
            return;

        // Do lots of function calls -- when the bug was present, each
        // of these calls would allocate a new temporary register. We can
        // detect profligate register allocation because it will substantially
        // curtail our recursion limit.
        g(); g(); g(); g(); g(); g(); g(); g(); g(); g();
        g(); g(); g(); g(); g(); g(); g(); g(); g(); g();
        g(); g(); g(); g(); g(); g(); g(); g(); g(); g();
        g(); g(); g(); g(); g(); g(); g(); g(); g(); g();
        g(); g(); g(); g(); g(); g(); g(); g(); g(); g();
        g(); g(); g(); g(); g(); g(); g(); g(); g(); g();
        g(); g(); g(); g(); g(); g(); g(); g(); g(); g();
        g(); g(); g(); g(); g(); g(); g(); g(); g(); g();
        g(); g(); g(); g(); g(); g(); g(); g(); g(); g();
        g(); g(); g(); g(); g(); g(); g(); g(); g(); g();
        g(); g(); g(); g(); g(); g(); g(); g(); g(); g();
        g(); g(); g(); g(); g(); g(); g(); g(); g(); g();
        g(); g(); g(); g(); g(); g(); g(); g(); g(); g();
        g(); g(); g(); g(); g(); g(); g(); g(); g(); g();
        g(); g(); g(); g(); g(); g(); g(); g(); g(); g();
        g(); g(); g(); g(); g(); g(); g(); g(); g(); g();

        f(g, ++x);
    })(function() {}, 0);
} catch(e) {
    message = "FAIL: Recursion threw an exception: " + e;
}

debug(message);
