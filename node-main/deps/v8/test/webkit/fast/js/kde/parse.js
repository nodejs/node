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

description("KDE JS Test");
// Check parsing of nested scopes; a couple of the break/continue to label cases are invalid.
shouldBe("function test() { return;}; lab: 1", "1");
shouldBe("function test() { while(0) break; } lab: 1", "1");
shouldBe("function test() { while(0) continue; } lab: 1", "1");

shouldBe("function test() { return lab;} lab: 1", "1");

shouldBe("function test() { return } lab: 1", "1");
shouldBe("function test() { while(0) break } lab: 1", "1");
shouldBe("function test() { while(0) continue } lab: 1", "1");

shouldBe("function test() { return 0 } lab: 1", "1");

a = 1
b = 123 // comment
c = 2
c = 3 /* comment */
d = 4

// non-ASCII identifier letters
shouldBe("var \u00E9\u0100\u02AF\u0388\u18A8 = 101; \u00E9\u0100\u02AF\u0388\u18A8;", "101");

// invalid identifier letters
shouldThrow("var f\xF7;");

// ASCII identifier characters as escape sequences
shouldBe("var \\u0061 = 102; a", "102");
shouldBe("var f\\u0030 = 103; f0", "103");

// non-ASCII identifier letters as escape sequences
shouldBe("var \\u00E9\\u0100\\u02AF\\u0388\\u18A8 = 104; \\u00E9\\u0100\\u02AF\\u0388\\u18A8;", "104");

// invalid identifier characters as escape sequences
shouldThrow("var f\\u00F7;");
shouldThrow("var \\u0030;");
shouldThrow("var test = { }; test.i= 0; test.i\\u002b= 1; test.i;");

shouldBe("var test = { }; test.i= 0; test.i\u002b= 1; test.i;", "1");
