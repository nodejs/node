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
"This test checks that functions on the array prototype correctly handle exceptions from property getters when called on non-array objects."
);

function test(f) {

    var testObj = {
        length: 3
    };
    var propertyGetter = {
        get: (function() { throw true; })
    }
    Object.defineProperty(testObj, 0, propertyGetter);
    Object.defineProperty(testObj, 1, propertyGetter);
    Object.defineProperty(testObj, 2, propertyGetter);

    try {
        f.call(testObj, function(){});
        return false;
    } catch (e) {
        return e === true;
    }
}

// This test makes sense for these functions: (they should get all properties on the array)
shouldBeTrue("test(Array.prototype.sort)");
shouldBeTrue("test(Array.prototype.every)");
shouldBeTrue("test(Array.prototype.some)");
shouldBeTrue("test(Array.prototype.forEach)");
shouldBeTrue("test(Array.prototype.map)");
shouldBeTrue("test(Array.prototype.filter)");
shouldBeTrue("test(Array.prototype.reduce)");
shouldBeTrue("test(Array.prototype.reduceRight)");

// Probably not testing much of anything in these cases, but make sure they don't crash!
shouldBeTrue("test(Array.prototype.join)");
shouldBeTrue("test(Array.prototype.pop)");
shouldBeFalse("test(Array.prototype.push)");
shouldBeTrue("test(Array.prototype.reverse)");
shouldBeTrue("test(Array.prototype.shift)");
shouldBeTrue("test(Array.prototype.slice)");
shouldBeTrue("test(Array.prototype.splice)");
shouldBeTrue("test(Array.prototype.unshift)");
shouldBeTrue("test(Array.prototype.indexOf)");
shouldBeTrue("test(Array.prototype.lastIndexOf)");
