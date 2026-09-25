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
"Test numeric escapes in string literals - https://bugs.webkit.org/show_bug.cgi?id=51724"
);

function test(_stringLiteral, _nonStrictResult, _strictResult)
{
    stringLiteral = '"' + _stringLiteral + '"';
    nonStrictResult = _nonStrictResult;
    shouldBe("eval(stringLiteral)", "nonStrictResult");

    stringLiteral = '"use strict"; ' + stringLiteral;
    if (_strictResult) {
        strictResult = _strictResult;
        shouldBe("eval(stringLiteral)", "strictResult");
    } else
        shouldThrow("eval(stringLiteral)");
}

// Tests for single digit octal and decimal escapes.
// In non-strict mode 0-7 are octal escapes, 8-9 are NonEscapeCharacters.
// In strict mode only "\0" is permitted.
test("\\0", "\x00", "\x00");
test("\\1", "\x01");
test("\\7", "\x07");
test("\\8", "8");
test("\\9", "9");

// Tests for multi-digit octal values outside strict mode;
// Octal literals may be 1-3 digits long.  In strict more all multi-digit sequences are illegal.
test("\\00", "\x00");
test("\\000", "\x00");
test("\\0000", "\x000");

test("\\01", "\x01");
test("\\001", "\x01");
test("\\0001", "\x001");

test("\\10", "\x08");
test("\\100", "\x40");
test("\\1000", "\x400");

test("\\19", "\x019");
test("\\109", "\x089");
test("\\1009", "\x409");

test("\\99", "99");
