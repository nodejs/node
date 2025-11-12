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
"Tests that constant folding an access to an uncaptured variable that is captured later in the same basic block doesn't lead to assertion failures."
);

var thingy = 456;

function bar() {
    return thingy;
}

function baz(a) {
    if (a) // Here we have an access to r2. The bug was concerned with our assertions thinking that this access was invalid.
        return arguments; // Force r2 (see below) to get captured.
}

function foo(p, a) {
    // The temporary variable corresponding to the 'bar' callee coming out of the ternary expression will be allocated by
    // the bytecompiler to some virtual register, say r2. This expression is engineered so that (1) the virtual register
    // chosen for the callee here is the same as the one that will be chosen for the first non-this argument below,
    // (2) that the callee ends up being constant but requires CFA to prove it, and (3) that we actually load that constant
    // using GetLocal (which happens because of the CheckFunction to check the callee).
    var x = (a + 1) + (p ? bar : bar)();
    // The temporary variable corresponding to the first non-this argument to baz will be allocated to the same virtual
    // register (i.e. r2).
    return baz(x);
}

for (var i = 0; i < 100; ++i)
    shouldBe("foo(true, 5)[0]", "462");
