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
"This test checks regular expressions using extended (> 255) characters and character classes."
);

// shouldThrow('var r = new RegExp("[\u0101-\u0100]"); r.exec("a")', 'null');

shouldBe('(new RegExp("[\u0100-\u0101]")).exec("a")', 'null');
shouldBe('(new RegExp("[\u0100]")).exec("a")', 'null');
shouldBe('(new RegExp("\u0100")).exec("a")', 'null');
shouldBe('(new RegExp("[\u0061]")).exec("a").toString()', '"a"');
shouldBe('(new RegExp("[\u0100-\u0101a]")).exec("a").toString()', '"a"');
shouldBe('(new RegExp("[\u0100a]")).exec("a").toString()', '"a"');
shouldBe('(new RegExp("\u0061")).exec("a").toString()', '"a"');
shouldBe('(new RegExp("[a-\u0100]")).exec("a").toString()', '"a"');
shouldBe('(new RegExp("[\u0100]")).exec("\u0100").toString()', '"\u0100"');
shouldBe('(new RegExp("[\u0100-\u0101]")).exec("\u0100").toString()', '"\u0100"');
shouldBe('(new RegExp("\u0100")).exec("\u0100").toString()', '"\u0100"');
