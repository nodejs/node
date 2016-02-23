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
'This is a test case for <a https://bugs.webkit.org/show_bug.cgi?id=64679">bug 64679</a>.'
);

// These calls pass undefined as this value, and as such should throw in toObject.
shouldThrow("Array.prototype.toString.call(undefined)");
shouldThrow("Array.prototype.toLocaleString.call(undefined)");
shouldThrow("Array.prototype.concat.call(undefined, [])");
shouldThrow("Array.prototype.join.call(undefined, [])");
shouldThrow("Array.prototype.pop.call(undefined)");
shouldThrow("Array.prototype.push.call(undefined, {})");
shouldThrow("Array.prototype.reverse.call(undefined)");
shouldThrow("Array.prototype.shift.call(undefined)");
shouldThrow("Array.prototype.slice.call(undefined, 0, 1)");
shouldThrow("Array.prototype.sort.call(undefined)");
shouldThrow("Array.prototype.splice.call(undefined, 0, 1)");
shouldThrow("Array.prototype.unshift.call(undefined, {})");
shouldThrow("Array.prototype.every.call(undefined, toString)");
shouldThrow("Array.prototype.forEach.call(undefined, toString)");
shouldThrow("Array.prototype.some.call(undefined, toString)");
shouldThrow("Array.prototype.indexOf.call(undefined, 0)");
shouldThrow("Array.prototype.indlastIndexOfexOf.call(undefined, 0)");
shouldThrow("Array.prototype.filter.call(undefined, toString)");
shouldThrow("Array.prototype.reduce.call(undefined, toString)");
shouldThrow("Array.prototype.reduceRight.call(undefined, toString)");
shouldThrow("Array.prototype.map.call(undefined, toString)");

// Test exception ordering in Array.prototype.toLocaleString ( https://bugs.webkit.org/show_bug.cgi?id=80663 )
shouldThrow("[{toLocaleString:function(){throw 1}},{toLocaleString:function(){throw 2}}].toLocaleString()", '1');
