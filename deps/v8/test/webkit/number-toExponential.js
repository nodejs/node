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

var posInf = 1/0;
var negInf = -1/0;
var nan = 0/0;

// From Acid3, http://bugs.webkit.org/show_bug.cgi?id=16640
shouldBeEqualToString("(0.0).toExponential(4)", "0.0000e+0");
shouldBeEqualToString("(-0.0).toExponential(4)", "0.0000e+0");
shouldBeEqualToString("(0.0).toExponential()", "0e+0");
shouldBeEqualToString("(-0.0).toExponential()", "0e+0");
// From http://bugs.webkit.org/show_bug.cgi?id=5259
shouldBeEqualToString("(123.456).toExponential()", "1.23456e+2");
shouldBeEqualToString("(123.456).toExponential(0)", "1e+2");
// 0 equivilents
shouldBeEqualToString("(123.456).toExponential(null)", "1e+2");
shouldBeEqualToString("(123.456).toExponential(false)", "1e+2");
shouldBeEqualToString("(123.456).toExponential('foo')", "1e+2");
shouldBeEqualToString("(123.456).toExponential(nan)", "1e+2"); // nan is treated like 0

shouldBeEqualToString("(123.456).toExponential(1)", "1.2e+2");
// 1 equivilents
shouldBeEqualToString("(123.456).toExponential(true)", "1.2e+2"); // just like 1
shouldBeEqualToString("(123.456).toExponential('1')", "1.2e+2");

shouldBeEqualToString("(123.456).toExponential(2)", "1.23e+2");
shouldBeEqualToString("(123.456).toExponential(2.9)", "1.23e+2");
shouldBeEqualToString("(123.456).toExponential(3)", "1.235e+2");
shouldBeEqualToString("(123.456).toExponential(5)", "1.23456e+2");
shouldBeEqualToString("(123.456).toExponential(6)", "1.234560e+2");
shouldBeEqualToString("(123.456).toExponential(20)", "1.23456000000000003070e+2");

// SpiderMonkey allows precision values 0 to 100, we only allow 0 to 20 currently
shouldThrow("(123.456).toExponential(21)");
shouldThrow("(123.456).toExponential(100)");
shouldThrow("(123.456).toExponential(101)");
shouldThrow("(123.456).toExponential(-1)");

shouldThrow("(1234.567).toExponential(posInf)");
shouldThrow("(1234.567).toExponential(negInf)");

shouldBeEqualToString("posInf.toExponential()", "Infinity");
shouldBeEqualToString("negInf.toExponential()", "-Infinity");
shouldBeEqualToString("nan.toExponential()", "NaN");

shouldBeEqualToString("(0.01).toExponential()", "1e-2");
shouldBeEqualToString("(0.1).toExponential()", "1e-1");
shouldBeEqualToString("(0.9).toExponential()", "9e-1");
shouldBeEqualToString("(0.9999).toExponential()", "9.999e-1");
shouldBeEqualToString("(0.9999).toExponential(2)", "1.00e+0");
