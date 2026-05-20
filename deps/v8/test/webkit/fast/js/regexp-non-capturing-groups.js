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
'Test for behavior of non-capturing groups, as described in <a href="http://blog.stevenlevithan.com/archives/npcg-javascript">' +
'a blog post by Steven Levithan</a> and <a href="http://bugs.webkit.org/show_bug.cgi?id=14931">bug 14931</a>.'
);

shouldBe('/(x)?\\1y/.test("y")', 'true');
shouldBe('/(x)?\\1y/.exec("y")', '["y", undefined]');
shouldBe('/(x)?y/.exec("y")', '["y", undefined]');
shouldBe('"y".match(/(x)?\\1y/)', '["y", undefined]');
shouldBe('"y".match(/(x)?y/)', '["y", undefined]');
shouldBe('"y".match(/(x)?\\1y/g)', '["y"]');
shouldBe('"y".split(/(x)?\\1y/)', '["", undefined, ""]');
shouldBe('"y".split(/(x)?y/)', '["", undefined, ""]');
shouldBe('"y".search(/(x)?\\1y/)', '0');
shouldBe('"y".replace(/(x)?\\1y/, "z")', '"z"');
shouldBe('"y".replace(/(x)?y/, "$1")', '""');
shouldBe('"y".replace(/(x)?\\1y/, function($0, $1){ return String($1); })', '"undefined"');
shouldBe('"y".replace(/(x)?y/, function($0, $1){ return String($1); })', '"undefined"');
shouldBe('"y".replace(/(x)?y/, function($0, $1){ return $1; })', '"undefined"');
