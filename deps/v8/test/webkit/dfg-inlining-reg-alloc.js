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
"This tests that register allocation still works under register pressure induced by inlining, out-of-line function calls (i.e. unconditional register flushing), and slow paths for object creation (i.e. conditional register flushing)."
);

// Inlineable constructor.
function foo(a, b, c) {
    this.a = a;
    this.b = b;
    this.c = c;
}

// Non-inlineable function. This relies on a size limit for inlining, but still
// produces integers. It also relies on the VM not reasoning about Math.log deeply
// enough to find some way of optimizing this code to be small enough to inline.
function bar(a, b) {
    a += b;
    a -= b;
    b ^= a;
    a += Math.log(b);
    b += a;
    b -= a;
    a ^= b;
    a += b;
    a -= b;
    b ^= a;
    a += Math.log(b);
    b += a;
    b -= a;
    a ^= b;
    a += b;
    a -= b;
    b ^= a;
    a += Math.log(b);
    b += a;
    b -= a;
    a ^= b;
    a += b;
    a -= b;
    b ^= a;
    a += Math.log(b);
    b += a;
    b -= a;
    a ^= b;
    a += b;
    a -= b;
    b ^= a;
    a += Math.log(b);
    b += a;
    b -= a;
    a ^= b;
    a += b;
    a -= b;
    b ^= a;
    a += Math.log(b);
    b += a;
    b -= a;
    a ^= b;
    a += b;
    a -= b;
    b ^= a;
    a += Math.log(b);
    b += a;
    b -= a;
    a ^= b;
    a += b;
    a -= b;
    b ^= a;
    a += Math.log(b);
    b += a;
    b -= a;
    a ^= b;
    a += b;
    a -= b;
    b ^= a;
    a += Math.log(b);
    b += a;
    b -= a;
    a ^= b;
    a += b;
    a -= b;
    b ^= a;
    a += Math.log(b);
    b += a;
    b -= a;
    a ^= b;
    a += b;
    a -= b;
    b ^= a;
    a += Math.log(b);
    b += a;
    b -= a;
    a ^= b;
    a += b;
    a -= b;
    b ^= a;
    a += Math.log(b);
    b += a;
    b -= a;
    a ^= b;
    a += b;
    a -= b;
    b ^= a;
    a += Math.log(b);
    b += a;
    b -= a;
    a ^= b;
    a += b;
    a -= b;
    b ^= a;
    a += Math.log(b);
    b += a;
    b -= a;
    a ^= b;
    a += b;
    a -= b;
    b ^= a;
    a += Math.log(b);
    b += a;
    b -= a;
    a ^= b;
    a += b;
    a -= b;
    b ^= a;
    a += Math.log(b);
    b += a;
    b -= a;
    a ^= b;
    return (a - b) | 0;
}

// Function into which we will inline foo but not bar.
function baz(a, b) {
    return new foo(bar(2 * a + 1, b - 1), bar(2 * a, b - 1), a);
}

// Do the test. It's crucial that o.a, o.b, and o.c are checked on each
// loop iteration.
for (var i = 0; i < 1000; ++i) {
    var o = baz(i, i + 1);
    shouldBe("o.a", "bar(2 * i + 1, i)");
    shouldBe("o.b", "bar(2 * i, i)");
    shouldBe("o.c", "i");
}
