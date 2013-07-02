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
"This tests that a call to array/string prototype methods pass the correct this value (undefined) to strict callees."
);

var undefinedString = String(undefined);
var globalObjectString = String(this);

function strictThrowThisString()
{
    "use strict";
    throw String(this);
}

function nonstrictThrowThisString()
{
    throw String(this);
}

function testArrayPrototypeSort(callback)
{
    try {
        [1,2].sort(callback);
    } catch (e) {
        return e;
    }
    return "FAILED";
}

function testArrayPrototypeFilter(callback)
{
    try {
        [1,2].filter(callback);
    } catch (e) {
        return e;
    }
    return "FAILED";
}

function testArrayPrototypeMap(callback)
{
    try {
        [1,2].map(callback);
    } catch (e) {
        return e;
    }
    return "FAILED";
}

function testArrayPrototypeEvery(callback)
{
    try {
        [1,2].every(callback);
    } catch (e) {
        return e;
    }
    return "FAILED";
}

function testArrayPrototypeForEach(callback)
{
    try {
        [1,2].forEach(callback);
    } catch (e) {
        return e;
    }
    return "FAILED";
}

function testArrayPrototypeSome(callback)
{
    try {
        [1,2].some(callback);
    } catch (e) {
        return e;
    }
    return "FAILED";
}

function testStringPrototypeReplace(callback)
{
    try {
        "1,2".replace('1', callback);
    } catch (e) {
        return e;
    }
    return "FAILED";
}

shouldBe('testArrayPrototypeSort(strictThrowThisString)', 'undefinedString');
shouldBe('testArrayPrototypeFilter(strictThrowThisString)', 'undefinedString');
shouldBe('testArrayPrototypeMap(strictThrowThisString)', 'undefinedString');
shouldBe('testArrayPrototypeEvery(strictThrowThisString)', 'undefinedString');
shouldBe('testArrayPrototypeForEach(strictThrowThisString)', 'undefinedString');
shouldBe('testArrayPrototypeSome(strictThrowThisString)', 'undefinedString');
shouldBe('testStringPrototypeReplace(strictThrowThisString)', 'undefinedString');

shouldBe('testArrayPrototypeSort(nonstrictThrowThisString)', 'globalObjectString');
shouldBe('testArrayPrototypeFilter(nonstrictThrowThisString)', 'globalObjectString');
shouldBe('testArrayPrototypeMap(nonstrictThrowThisString)', 'globalObjectString');
shouldBe('testArrayPrototypeEvery(nonstrictThrowThisString)', 'globalObjectString');
shouldBe('testArrayPrototypeForEach(nonstrictThrowThisString)', 'globalObjectString');
shouldBe('testArrayPrototypeSome(nonstrictThrowThisString)', 'globalObjectString');
shouldBe('testStringPrototypeReplace(nonstrictThrowThisString)', 'globalObjectString');
