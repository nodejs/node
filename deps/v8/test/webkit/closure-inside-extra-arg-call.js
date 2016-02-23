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

"This test checks that activation objects for functions called with too many arguments are created properly."

);


var c1;

function f1()
{
    var a = "x";
    var b = "y";
    var c = a + b;
    var d = a + b + c;

    c1 = function() { return d; }
}

f1(0, 0, 0, 0, 0, 0, 0, 0, 0);

function s1() {
    shouldBe("c1()", '"xyxy"');
}

function t1() {
    var a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p;
    s1();
}

t1();

var c2;

function f2()
{
    var a = "x";
    var b = "y";
    var c = a + b;
    var d = a + b + c;

    c2 = function() { return d; }
}

new f2(0, 0, 0, 0, 0, 0, 0, 0, 0);

function s2() {
    shouldBe("c2()", '"xyxy"');
}

function t2() {
    var a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p;
    s2();
}

t2();
