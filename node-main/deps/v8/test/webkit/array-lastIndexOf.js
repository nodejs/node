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
'This test checks lastIndexOf for various values in an array'
);


var testArray = [2, 5, 9, 2];
var lastIndex = 0;

lastIndex = testArray.lastIndexOf(2,-500);
shouldBe('lastIndex', '-1');
lastIndex = testArray.lastIndexOf(9,500);
shouldBe('lastIndex', '2');
lastIndex = testArray.lastIndexOf(2);
shouldBe('lastIndex', '3');
lastIndex = testArray.lastIndexOf(7);
shouldBe('lastIndex', '-1');
lastIndex = testArray.lastIndexOf(2, 3);
shouldBe('lastIndex', '3');
lastIndex = testArray.lastIndexOf(2, 2);
shouldBe('lastIndex', '0');
lastIndex = testArray.lastIndexOf(2, -2);
shouldBe('lastIndex', '0');
lastIndex = testArray.lastIndexOf(2, -1);
shouldBe('lastIndex', '3');

delete testArray[1];

lastIndex = testArray.lastIndexOf(undefined);
shouldBe('lastIndex', '-1');

delete testArray[3];

lastIndex = testArray.lastIndexOf(undefined);
shouldBe('lastIndex', '-1');

testArray = new Array(20);

lastIndex = testArray.lastIndexOf(undefined);
shouldBe('lastIndex', '-1');

testArray[19] = undefined;

lastIndex = testArray.lastIndexOf(undefined);
shouldBe('lastIndex', '19');

lastIndex = testArray.lastIndexOf(undefined, 18);
shouldBe('lastIndex', '-1');

delete testArray[19];

lastIndex = testArray.lastIndexOf(undefined);
shouldBe('lastIndex', '-1');
