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
'Test regular expression processing with alternatives.'
);

var s1 = "<p>content</p>";
shouldBe('s1.match(/<((\\/([^>]+)>)|(([^>]+)>))/)', '["<p>","p>",undefined,undefined,"p>","p"]');
shouldBe('s1.match(/<((ABC>)|(\\/([^>]+)>)|(([^>]+)>))/)', '["<p>","p>",undefined,undefined,undefined,"p>","p"]');
shouldBe('s1.match(/<(a|\\/p|.+?)>/)', '["<p>","p"]');

// Force YARR to use Interpreter by using iterative parentheses
shouldBe('s1.match(/<((\\/([^>]+)>)|((([^>])+)>))/)', '["<p>","p>",undefined,undefined,"p>","p","p"]');
shouldBe('s1.match(/<((ABC>)|(\\/([^>]+)>)|((([^>])+)>))/)', '["<p>","p>",undefined,undefined,undefined,"p>","p","p"]');
shouldBe('s1.match(/<(a|\\/p|(.)+?)>/)', '["<p>","p","p"]');

// Force YARR to use Interpreter by using backreference
var s2 = "<p>p</p>";
shouldBe('s2.match(/<((\\/([^>]+)>)|(([^>]+)>))\\5/)', '["<p>p","p>",undefined,undefined,"p>","p"]');
shouldBe('s2.match(/<((ABC>)|(\\/([^>]+)>)|(([^>]+)>))\\6/)', '["<p>p","p>",undefined,undefined,undefined,"p>","p"]');
shouldBe('s2.match(/<(a|\\/p|.+?)>\\1/)', '["<p>p","p"]');
