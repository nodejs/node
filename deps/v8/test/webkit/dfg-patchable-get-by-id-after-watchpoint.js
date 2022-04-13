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
"This tests that a patchable GetById right after a watchpoint has the appropriate nop padding."
);

function foo(o, p) {
    var a = p.f;
    var b = o.f; // Watchpoint.
    var c = p.g; // Patchable GetById.
    return b(a + c);
}

function O() {
}

O.prototype.f = function(x) { return x + 1; };

var o = new O();

function P1() {
}

P1.prototype.g = 42;

function P2() {
}

P2.prototype.g = 24;

var p1 = new P1();
var p2 = new P2();

p1.f = 1;
p2.f = 2;

for (var i = 0; i < 200; ++i) {
    var p = (i % 2) ? p1 : p2;
    var expected = (i % 2) ? 44 : 27;
    if (i == 150) {
        // Cause first the watchpoint on o.f to fire, and then the GetById
        // to be reset.
        O.prototype.g = 57; // Fire the watchpoint.
        P1.prototype.h = 58; // Reset the GetById.
        P2.prototype.h = 59; // Not necessary, but what the heck - this resets the GetById even more.
    }
    shouldBe("foo(o, p)", "" + expected);
}
