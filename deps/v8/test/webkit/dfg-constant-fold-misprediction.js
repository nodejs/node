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
"This tests that a constant folding on a node that has obviously mispredicted type doesn't send the compiler into an infinite loop."
);

// A function with an argument correctly predicted double.
function foo(x) {
    // Two variables holding constants such that the bytecode generation constant folder
    // will not constant fold the division below, but the DFG constant folder will.
    var a = 1;
    var b = 4000;
    // A division that is going to be predicted integer on the first compilation. The
    // compilation will be triggered from the loop below so the slow case counter of the
    // division will be 1, which is too low for the division to be predicted double.
    // If we constant fold this division, we'll have a constant node that is predicted
    // integer but that contains a double. The subsequent addition to x, which is
    // predicted double, will lead the Fixup phase to inject an Int32ToDouble node on
    // the constant-that-was-a-division; subsequent fases in the fixpoint will constant
    // fold that Int32ToDouble. And hence we will have an infinite loop. The correct fix
    // is to disable constant folding of mispredicted nodes; that allows the normal
    // process of correcting predictions (OSR exit profiling, exiting to profiled code,
    // and recompilation with exponential backoff) to take effect so that the next
    // compilation does not make this same mistake.
    var c = (a / b) + x;
    // A pointless loop to force the first compilation to occur before the division got
    // hot. If this loop was not here then the division would be known to produce doubles
    // on the first compilation.
    var d = 0;
    for (var i = 0; i < 1000; ++i)
        d++;
    return c + d;
}

// Call foo() enough times to make totally sure that we optimize.
for (var i = 0; i < 5; ++i)
    shouldBe("foo(0.5)", "1000.50025");
