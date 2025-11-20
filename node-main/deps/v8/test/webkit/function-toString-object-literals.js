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
"This test checks that object literals are serialized properly. " +
"It's needed in part because JavaScriptCore converts numeric property names to string and back."
);

function compileAndSerialize(expression)
{
    var f = eval("(function () { return " + expression + "; })");
    var serializedString = f.toString();
    serializedString = serializedString.replace(/[ \t\r\n]+/g, " ");
    serializedString = serializedString.replace("function () { return ", "");
    serializedString = serializedString.replace("; }", "");
    return serializedString;
}

shouldBe("compileAndSerialize('a = { 1: null }')", "'a = { 1: null }'");
shouldBe("compileAndSerialize('a = { 0: null }')", "'a = { 0: null }'");
shouldBe("compileAndSerialize('a = { 1.0: null }')", "'a = { 1.0: null }'");
shouldBe("compileAndSerialize('a = { \"1.0\": null }')", "'a = { \"1.0\": null }'");
shouldBe("compileAndSerialize('a = { 1e-500: null }')", "'a = { 1e-500: null }'");
shouldBe("compileAndSerialize('a = { 1e-300: null }')", "'a = { 1e-300: null }'");
shouldBe("compileAndSerialize('a = { 1e300: null }')", "'a = { 1e300: null }'");
shouldBe("compileAndSerialize('a = { 1e500: null }')", "'a = { 1e500: null }'");

shouldBe("compileAndSerialize('a = { NaN: null }')", "'a = { NaN: null }'");
shouldBe("compileAndSerialize('a = { Infinity: null }')", "'a = { Infinity: null }'");

shouldBe("compileAndSerialize('a = { \"1\": null }')", "'a = { \"1\": null }'");
shouldBe("compileAndSerialize('a = { \"1hi\": null }')", "'a = { \"1hi\": null }'");
shouldBe("compileAndSerialize('a = { \"\\\'\": null }')", "'a = { \"\\\'\": null }'");
shouldBe("compileAndSerialize('a = { \"\\\\\"\": null }')", "'a = { \"\\\\\"\": null }'");

shouldBe("compileAndSerialize('a = { get x() { } }')", "'a = { get x() { } }'");
shouldBe("compileAndSerialize('a = { set x(y) { } }')", "'a = { set x(y) { } }'");

shouldThrow("compileAndSerialize('a = { --1: null }')");
shouldThrow("compileAndSerialize('a = { -NaN: null }')");
shouldThrow("compileAndSerialize('a = { -0: null }')");
shouldThrow("compileAndSerialize('a = { -0.0: null }')");
shouldThrow("compileAndSerialize('a = { -Infinity: null }')");
