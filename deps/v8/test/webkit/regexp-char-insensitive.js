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
"This test checks the case-insensitive matching of character literals."
);

shouldBeTrue("/\u00E5/i.test('/\u00E5/')");
shouldBeTrue("/\u00E5/i.test('/\u00C5/')");
shouldBeTrue("/\u00C5/i.test('/\u00E5/')");
shouldBeTrue("/\u00C5/i.test('/\u00C5/')");

shouldBeFalse("/\u00E5/i.test('P')");
shouldBeFalse("/\u00E5/i.test('PASS')");
shouldBeFalse("/\u00C5/i.test('P')");
shouldBeFalse("/\u00C5/i.test('PASS')");

shouldBeNull("'PASS'.match(/\u00C5/i)");
shouldBeNull("'PASS'.match(/\u00C5/i)");

shouldBe("'PAS\u00E5'.replace(/\u00E5/ig, 'S')", "'PASS'");
shouldBe("'PAS\u00E5'.replace(/\u00C5/ig, 'S')", "'PASS'");
shouldBe("'PAS\u00C5'.replace(/\u00E5/ig, 'S')", "'PASS'");
shouldBe("'PAS\u00C5'.replace(/\u00C5/ig, 'S')", "'PASS'");

shouldBe("'PASS'.replace(/\u00E5/ig, '%C3%A5')", "'PASS'");
shouldBe("'PASS'.replace(/\u00C5/ig, '%C3%A5')", "'PASS'");
