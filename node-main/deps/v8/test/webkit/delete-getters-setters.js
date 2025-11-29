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
"This test checks that deletion of properties works properly with getters and setters."
);

b1 = 1;
this.__defineSetter__("a1", function() {});
this.__defineSetter__("b1", function() {});
delete a1;
shouldThrow("b1.property");

a2 = 1;
this.__defineSetter__("a2", function() {});
this.__defineSetter__("b2", function() {});
delete b2;
shouldThrow("a2.property");

b3 = 1;
this.__defineGetter__("a3", function() {});
this.__defineGetter__("b3", function() {});
delete a3;
shouldThrow("b3.property");

a4 = 1;
this.__defineGetter__("a4", function() {});
this.__defineGetter__("b4", function() {});
delete b4;
shouldThrow("a4.property");

b5 = 1;
this.__defineSetter__("a5", function() {});
this.__defineGetter__("b5", function() {});
delete a5;
shouldThrow("b5.property");

a6 = 1;
this.__defineSetter__("a6", function() {});
this.__defineGetter__("b6", function() {});
delete b6;
shouldThrow("a6.property");

b7 = 1;
this.__defineGetter__("a7", function() {});
this.__defineSetter__("b7", function() {});
delete a7;
shouldThrow("b7.property");

a8 = 1;
this.__defineGetter__("a8", function() {});
this.__defineSetter__("b8", function() {});
delete b8;
shouldThrow("a8.property");

var o1 = { b: 1 };
o1.__defineSetter__("a", function() {});
o1.__defineSetter__("b", function() {});
delete o1.a;
shouldThrow("o1.b.property");

var o2 = { a: 1 };
o2.__defineSetter__("a", function() {});
o2.__defineSetter__("b", function() {});
delete o2.b;
shouldThrow("o1.a.property");

var o3 = { b: 1 };
o3.__defineGetter__("a", function() {});
o3.__defineGetter__("b", function() {});
delete o3.a;
shouldThrow("o3.b.property");

var o4 = { a: 1 };
o4.__defineGetter__("a", function() {});
o4.__defineGetter__("b", function() {});
delete o4.b;
shouldThrow("o4.a.property");

var o5 = { b: 1 };
o5.__defineSetter__("a", function() {});
o5.__defineSetter__("b", function() {});
delete o5.a;
shouldThrow("o5.b.property");

var o6 = { a: 1 };
o6.__defineSetter__("a", function() {});
o6.__defineSetter__("b", function() {});
delete o6.b;
shouldThrow("o6.a.property");

var o7 = { b: 1 };
o7.__defineGetter__("a", function() {});
o7.__defineGetter__("b", function() {});
delete o7.a;
shouldThrow("o7.b.property");

var o8 = { a: 1 };
o8.__defineGetter__("a", function() {});
o8.__defineGetter__("b", function() {});
delete o8.b;
shouldThrow("o8.a.property");
