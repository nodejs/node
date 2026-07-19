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
"This tests that inlining a function that does not use this does not result in this being lost entirely."
);

function foo(a, b) {
    return a + b;
}

function bar(a, b) {
    return this.f + a + b;
}

function baz(o, a, b) {
    return o.stuff(a, b);
}

var functionToCall = foo;
var offset = 0;
for (var i = 0; i < 1000; ++i) {
    if (i == 600) {
        functionToCall = bar;
        offset = 42;
    }

    // Create some bizarre object to prevent method_check optimizations, since those will result in
    // a failure while the function is still live.
    var object = {};
    object["a" + i] = i;
    object.stuff = functionToCall;
    object.f = 42;

    shouldBe("baz(object, " + i + ", " + (i * 2) + ")", "" + (offset + i + i * 2));
}
