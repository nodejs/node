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

description("Make sure that we correctly identify parse errors in object literals");

shouldThrow("({a:1, get a(){}})");
shouldThrow("({a:1, set a(v){}})");
shouldThrow("({get a(){}, a:1})");
shouldThrow("({set a(v){}, a:1})");
shouldThrow("({get a(){}, get a(){}})");
shouldThrow("({set a(v){}, set a(v){}})");
shouldThrow("({set a(v){}, get a(){}, set a(v){}})");
shouldThrow("(function(){({a:1, get a(){}})})");
shouldThrow("(function(){({a:1, set a(v){}})})");
shouldThrow("(function(){({get a(){}, a:1})})");
shouldThrow("(function(){({set a(v){}, a:1})})");
shouldThrow("(function(){({get a(){}, get a(){}})})");
shouldThrow("(function(){({set a(v){}, set a(v){}})})");
shouldThrow("(function(){({set a(v){}, get a(){}, set a(v){}})})");
shouldBeTrue("({a:1, a:1, a:1}), true");
shouldBeTrue("({get a(){}, set a(v){}}), true");
shouldBeTrue("({set a(v){}, get a(){}}), true");
shouldBeTrue("(function(){({a:1, a:1, a:1})}), true");
shouldBeTrue("(function(){({get a(){}, set a(v){}})}), true");
shouldBeTrue("(function(){({set a(v){}, get a(){}})}), true");
