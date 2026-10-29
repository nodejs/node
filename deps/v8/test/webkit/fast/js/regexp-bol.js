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
'Test for beginning of line (BOL or ^) matching</a>'
);

var s = "abc123def456xyzabc789abc999";
shouldBeNull('s.match(/^notHere/)');
shouldBe('s.match(/^abc/)', '["abc"]');
shouldBe('s.match(/(^|X)abc/)', '["abc",""]');
shouldBe('s.match(/^longer|123/)', '["123"]');
shouldBe('s.match(/(^abc|c)123/)', '["abc123","abc"]');
shouldBe('s.match(/(c|^abc)123/)', '["abc123","abc"]');
shouldBe('s.match(/(^ab|abc)123/)', '["abc123","abc"]');
shouldBe('s.match(/(bc|^abc)([0-9]*)a/)', '["bc789a","bc","789"]');
shouldBeNull('/(?:(Y)X)|(X)/.exec("abc")');
shouldBeNull('/(?:(?:^|Y)X)|(X)/.exec("abc")');
shouldBeNull('/(?:(?:^|Y)X)|(X)/.exec("abcd")');
shouldBe('/(?:(?:^|Y)X)|(X)/.exec("Xabcd")', '["X",undefined]');
shouldBe('/(?:(?:^|Y)X)|(X)/.exec("aXbcd")', '["X","X"]');
shouldBe('/(?:(?:^|Y)X)|(X)/.exec("abXcd")', '["X","X"]');
shouldBe('/(?:(?:^|Y)X)|(X)/.exec("abcXd")', '["X","X"]');
shouldBe('/(?:(?:^|Y)X)|(X)/.exec("abcdX")', '["X","X"]');
shouldBe('/(?:(?:^|Y)X)|(X)/.exec("YXabcd")', '["YX",undefined]');
shouldBe('/(?:(?:^|Y)X)|(X)/.exec("aYXbcd")', '["YX",undefined]');
shouldBe('/(?:(?:^|Y)X)|(X)/.exec("abYXcd")', '["YX",undefined]');
shouldBe('/(?:(?:^|Y)X)|(X)/.exec("abcYXd")', '["YX",undefined]');
shouldBe('/(?:(?:^|Y)X)|(X)/.exec("abcdYX")', '["YX",undefined]');
