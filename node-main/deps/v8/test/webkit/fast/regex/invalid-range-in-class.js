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
"This page tests invalid character ranges in character classes."
);

// These test a basic range / non range.
shouldBe('/[a-c]+/.exec("-acbd");', '["acb"]');
shouldBe('/[a\\-c]+/.exec("-acbd")', '["-ac"]');

// A reverse-range is invalid.
shouldThrow('/[c-a]+/.exec("-acbd");');

// A character-class in a range is invalid, according to ECMA-262, but we allow it.
shouldBe('/[\\d-x]+/.exec("1-3xy");', '["1-3x"]');
shouldBe('/[x-\\d]+/.exec("1-3xy");', '["1-3x"]');
shouldBe('/[\\d-\\d]+/.exec("1-3xy");', '["1-3"]');

// Whilst we break with ECMA-262's definition of CharacterRange, we do comply with
// the grammar, and as such in the following regex a-z cannot be matched as a range.
shouldBe('/[\\d-a-z]+/.exec("az1-3y");', '["az1-3"]');

// An escaped hypen should not be confused for an invalid range.
shouldBe('/[\\d\\-x]+/.exec("1-3xy");', '["1-3x"]');
shouldBe('/[x\\-\\d]+/.exec("1-3xy");', '["1-3x"]');
shouldBe('/[\\d\\-\\d]+/.exec("1-3xy");', '["1-3"]');

// A hyphen after a character-class is not invalid.
shouldBe('/[\\d-]+/.exec("1-3xy")', '["1-3"]');
