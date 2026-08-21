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
"This page tests that a RegExp object's lastIndex behaves like a regular property."
);

// lastIndex is not configurable
shouldBeFalse("delete /x/.lastIndex");
shouldThrow("'use strict'; delete /x/.lastIndex");

// lastIndex is not enumerable
shouldBeTrue("'lastIndex' in /x/");
shouldBeTrue("for (property in /x/) if (property === 'lastIndex') throw false; true");

// lastIndex is writable
shouldBeTrue("var re = /x/; re.lastIndex = re; re.lastIndex === re");

// Cannot redefine lastIndex as an accessor
shouldThrow("Object.defineProperty(/x/, {get:function(){}})");

// Cannot redefine lastIndex as enumerable
shouldThrow("Object.defineProperty(/x/, 'lastIndex', {enumerable:true}); true");
shouldBeTrue("Object.defineProperty(/x/, 'lastIndex', {enumerable:false}); true");

// Cannot redefine lastIndex as configurable
shouldThrow("Object.defineProperty(/x/, 'lastIndex', {configurable:true}); true");
shouldBeTrue("Object.defineProperty(/x/, 'lastIndex', {configurable:false}); true");

// Can redefine lastIndex as read-only
shouldBe("var re = Object.defineProperty(/x/, 'lastIndex', {writable:true}); re.lastIndex = 42; re.lastIndex", '42');
shouldBe("var re = Object.defineProperty(/x/, 'lastIndex', {writable:false}); re.lastIndex = 42; re.lastIndex", '0');

// Can redefine the value
shouldBe("var re = Object.defineProperty(/x/, 'lastIndex', {value:42}); re.lastIndex", '42');

// Cannot redefine read-only lastIndex as writable
shouldThrow("Object.defineProperty(Object.defineProperty(/x/, 'lastIndex', {writable:false}), 'lastIndex', {writable:true}); true");

// Can only redefine the value of a read-only lastIndex as the current value
shouldThrow("Object.defineProperty(Object.defineProperty(/x/, 'lastIndex', {writable:false}), 'lastIndex', {value:42}); true");
shouldBeTrue("Object.defineProperty(Object.defineProperty(/x/, 'lastIndex', {writable:false}), 'lastIndex', {value:0}); true");

// Trying to run a global regular expression should throw, if lastIndex is read-only
shouldBe("Object.defineProperty(/x/, 'lastIndex', {writable:false}).exec('')", 'null');
shouldBe("Object.defineProperty(/x/, 'lastIndex', {writable:false}).exec('x')", '["x"]');
shouldThrow("Object.defineProperty(/x/g, 'lastIndex', {writable:false}).exec('')");
shouldThrow("Object.defineProperty(/x/g, 'lastIndex', {writable:false}).exec('x')");

// Should be able to freeze a regular expression object.
shouldBeTrue("var re = /x/; Object.freeze(re); Object.isFrozen(re);");
