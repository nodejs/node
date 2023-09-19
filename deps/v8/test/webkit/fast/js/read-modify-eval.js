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
'Tests whether eval() works inside statements that read and modify a value.'
);

function multTest()
{
    var x = 1;
    x *= eval('2');
    return x == 2;
}

function divTest()
{
    var x = 2;
    x /= eval('2');
    return x == 1;
}

function addTest()
{
    var x = 0;
    x += eval('1');
    return x == 1;
}

function subTest()
{
    var x = 0;
    x -= eval('1');
    return x == -1;
}

function lshiftTest()
{
    var x = 1;
    x <<= eval('1');
    return x == 2;
}

function rshiftTest()
{
    var x = 1;
    x >>= eval('1');
    return x == 0;
}

function urshiftTest()
{
    var x = 1;
    x >>>= eval('1');
    return x == 0;
}

function andTest()
{
    var x = 1;
    x &= eval('1');
    return x == 1;
}

function xorTest()
{
    var x = 0;
    x ^= eval('1');
    return x == 1;
}

function orTest()
{
    var x = 0;
    x |= eval('1');
    return x == 1;
}

function modTest()
{
    var x = 4;
    x %= eval('3');
    return x == 1;
}

function preIncTest()
{
    var x = { value: 0 };
    ++eval('x').value;
    return x.value == 1;
}

function preDecTest()
{
    var x = { value: 0 };
    --eval('x').value;
    return x.value == -1;
}

function postIncTest()
{
    var x = { value: 0 };
    eval('x').value++;
    return x.value == 1;
}

function postDecTest()
{
    var x = { value: 0 };
    eval('x').value--;
    return x.value == -1;
}

function primitiveThisTest()
{
    // Test that conversion of this is persistent over multiple calls to eval,
    // even where 'this' is not directly used within the function.
    eval('this.value = "Seekrit message";');
    return eval('this.value') === "Seekrit message";
}

function strictThisTest()
{
    // In a strict mode function primitive this values are not converted, so
    // the property access in the first eval is writing a value to a temporary
    // object. This throws, per section 8.7.2.
    "use strict";
    eval('this.value = "Seekrit message";');
    return eval('this.value') === undefined;
}

shouldBeTrue('multTest();');
shouldBeTrue('divTest();');
shouldBeTrue('addTest();');
shouldBeTrue('subTest();');
shouldBeTrue('lshiftTest();');
shouldBeTrue('rshiftTest();');
shouldBeTrue('urshiftTest();');
shouldBeTrue('andTest();');
shouldBeTrue('xorTest();');
shouldBeTrue('orTest();');
shouldBeTrue('modTest();');

shouldBeTrue('preIncTest();');
shouldBeTrue('preDecTest();');
shouldBeTrue('postIncTest();');
shouldBeTrue('postDecTest();');

shouldBeTrue('primitiveThisTest.call(1);');
shouldThrow('strictThisTest.call(1);');
