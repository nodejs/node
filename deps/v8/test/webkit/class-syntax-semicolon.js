// Copyright 2015 the V8 project authors. All rights reserved.
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

description('Tests for ES6 class syntax containing semicolon in the class body');

shouldThrow("class A { foo() ; { } }", "'SyntaxError: Unexpected token ;'");
shouldThrow("class A { get foo;() { } }", "'SyntaxError: Unexpected token ;'");
shouldThrow("class A { get foo() ; { } }", "'SyntaxError: Unexpected token ;'");
shouldThrow("class A { set foo;(x) { } }", "'SyntaxError: Unexpected token ;'");
shouldThrow("class A { set foo(x) ; { } }", "'SyntaxError: Unexpected token ;'");

shouldNotThrow("class A { ; }");
shouldNotThrow("class A { foo() { } ; }");
shouldNotThrow("class A { get foo() { } ; }");
shouldNotThrow("class A { set foo(x) { } ; }");
shouldNotThrow("class A { static foo() { } ; }");
shouldNotThrow("class A { static get foo() { } ; }");
shouldNotThrow("class A { static set foo(x) { } ; }");

shouldNotThrow("class A { ; foo() { } }");
shouldNotThrow("class A { ; get foo() { } }");
shouldNotThrow("class A { ; set foo(x) { } }");
shouldNotThrow("class A { ; static foo() { } }");
shouldNotThrow("class A { ; static get foo() { } }");
shouldNotThrow("class A { ; static set foo(x) { } }");

shouldNotThrow("class A { foo() { } ; foo() {} }");
shouldNotThrow("class A { foo() { } ; get foo() {} }");
shouldNotThrow("class A { foo() { } ; set foo(x) {} }");
shouldNotThrow("class A { foo() { } ; static foo() {} }");
shouldNotThrow("class A { foo() { } ; static get foo() {} }");
shouldNotThrow("class A { foo() { } ; static set foo(x) {} }");

shouldNotThrow("class A { get foo() { } ; foo() {} }");
shouldNotThrow("class A { get foo() { } ; get foo() {} }");
shouldNotThrow("class A { get foo() { } ; set foo(x) {} }");
shouldNotThrow("class A { get foo() { } ; static foo() {} }");
shouldNotThrow("class A { get foo() { } ; static get foo() {} }");
shouldNotThrow("class A { get foo() { } ; static set foo(x) {} }");

shouldNotThrow("class A { set foo(x) { } ; foo() {} }");
shouldNotThrow("class A { set foo(x) { } ; get foo() {} }");
shouldNotThrow("class A { set foo(x) { } ; set foo(x) {} }");
shouldNotThrow("class A { set foo(x) { } ; static foo() {} }");
shouldNotThrow("class A { set foo(x) { } ; static get foo() {} }");
shouldNotThrow("class A { set foo(x) { } ; static set foo(x) {} }");

shouldNotThrow("class A { static foo() { } ; foo() {} }");
shouldNotThrow("class A { static foo() { } ; get foo() {} }");
shouldNotThrow("class A { static foo() { } ; set foo(x) {} }");
shouldNotThrow("class A { static foo() { } ; static foo() {} }");
shouldNotThrow("class A { static foo() { } ; static get foo() {} }");
shouldNotThrow("class A { static foo() { } ; static set foo(x) {} }");

shouldNotThrow("class A { static get foo() { } ; foo() {} }");
shouldNotThrow("class A { static get foo() { } ; get foo() {} }");
shouldNotThrow("class A { static get foo() { } ; set foo(x) {} }");
shouldNotThrow("class A { static get foo() { } ; static foo() {} }");
shouldNotThrow("class A { static get foo() { } ; static get foo() {} }");
shouldNotThrow("class A { static get foo() { } ; static set foo(x) {} }");

shouldNotThrow("class A { static set foo(x) { } ; foo() {} }");
shouldNotThrow("class A { static set foo(x) { } ; get foo() {} }");
shouldNotThrow("class A { static set foo(x) { } ; set foo(x) {} }");
shouldNotThrow("class A { static set foo(x) { } ; static foo() {} }");
shouldNotThrow("class A { static set foo(x) { } ; static get foo() {} }");
shouldNotThrow("class A { static set foo(x) { } ; static set foo(x) {} }");

var successfullyParsed = true;
