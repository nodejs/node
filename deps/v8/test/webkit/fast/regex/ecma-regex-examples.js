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
"This page tests the regex examples from the ECMA-262 specification."
);

var regex01 = /a|ab/;
shouldBe('regex01.exec("abc")', '["a"]');

var regex02 = /((a)|(ab))((c)|(bc))/;
shouldBe('regex02.exec("abc")', '["abc", "a", "a", undefined, "bc", undefined, "bc"]');

var regex03 = /a[a-z]{2,4}/;
shouldBe('regex03.exec("abcdefghi")', '["abcde"]');

var regex04 = /a[a-z]{2,4}?/;
shouldBe('regex04.exec("abcdefghi")', '["abc"]');

var regex05 = /(aa|aabaac|ba|b|c)*/;
shouldBe('regex05.exec("aabaac")', '["aaba", "ba"]');

var regex06 = /^(a+)\1*,\1+$/;
shouldBe('"aaaaaaaaaa,aaaaaaaaaaaaaaa".replace(regex06,"$1")', '"aaaaa"');

var regex07 = /(z)((a+)?(b+)?(c))*/;
shouldBe('regex07.exec("zaacbbbcac")', '["zaacbbbcac", "z", "ac", "a", undefined, "c"]');

var regex08 = /(a*)*/;
shouldBe('regex08.exec("b")', '["", undefined]');

var regex09 = /(a*)b\1+/;
shouldBe('regex09.exec("baaaac")', '["b", ""]');

var regex10 = /(?=(a+))/;
shouldBe('regex10.exec("baaabac")', '["", "aaa"]');

var regex11 = /(?=(a+))a*b\1/;
shouldBe('regex11.exec("baaabac")', '["aba", "a"]');

var regex12 = /(.*?)a(?!(a+)b\2c)\2(.*)/;
shouldBe('regex12.exec("baaabaac")', '["baaabaac", "ba", undefined, "abaac"]');
