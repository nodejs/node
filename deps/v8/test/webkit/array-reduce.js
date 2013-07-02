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
"This test checks the behavior of the reduce() method on a number of objects."
);

function toObject(array) {
    var o = {};
    for (var i in array)
        o[i] = array[i];
    o.length = array.length;
    o.reduce = Array.prototype.reduce;
    return o;
}
function toUnorderedObject(array) {
    var o = {};
    var props = [];
    for (var i in array)
        props.push(i);
    for (var i = props.length - 1; i >= 0; i--)
        o[props[i]] = array[props[i]];
    o.length = array.length;
    o.reduce = Array.prototype.reduce;
    return o;
}

shouldBe("[0,1,2,3].reduce(function(a,b){ return a + b; })", "6");
shouldBe("[1,2,3].reduce(function(a,b){ return a + b; })", "6");
shouldBe("[0,1,2,3].reduce(function(a,b){ return a + b; }, 4)", "10");
shouldBe("[1,2,3].reduce(function(a,b){ return a + b; }, 4)", "10");
shouldBe("toObject([0,1,2,3]).reduce(function(a,b){ return a + b; })", "6");
shouldBe("toObject([1,2,3]).reduce(function(a,b){ return a + b; })", "6");
shouldBe("toObject([0,1,2,3]).reduce(function(a,b){ return a + b; }, 4)", "10");
shouldBe("toObject([1,2,3]).reduce(function(a,b){ return a + b; }, 4)", "10");
shouldBe("toUnorderedObject([0,1,2,3]).reduce(function(a,b){ return a + b; })", "6");
shouldBe("toUnorderedObject([1,2,3]).reduce(function(a,b){ return a + b; })", "6");
shouldBe("toUnorderedObject([0,1,2,3]).reduce(function(a,b){ return a + b; }, 4)", "10");
shouldBe("toUnorderedObject([1,2,3]).reduce(function(a,b){ return a + b; }, 4)", "10");
sparseArray = [];
sparseArray[10] = 10;
shouldBe("sparseArray.reduce(function(a,b){ return a + b; }, 0)", "10");
shouldBe("toObject(sparseArray).reduce(function(a,b){ return a + b; }, 0)", "10");
var callCount = 0;
shouldBe("sparseArray.reduce(function(a,b){ callCount++; }); callCount", "0");
callCount = 0;
shouldBe("toObject(sparseArray).reduce(function(a,b){ callCount++; }); callCount", "0");
var callCount = 0;
shouldBe("sparseArray.reduce(function(a,b){ callCount++; }, 0); callCount", "1");
callCount = 0;
shouldBe("toObject(sparseArray).reduce(function(a,b){ callCount++; }, 0); callCount", "1");
callCount = 0;
shouldBe("[0,1,2,3,4].reduce(function(a,b){ callCount++; }, 0); callCount", "5");
callCount = 0;
shouldBe("[0,1,2,3,4].reduce(function(a,b){ callCount++; }); callCount", "4");
callCount = 0;
shouldBe("[1, 2, 3, 4].reduce(function(a,b, i, thisObj){ thisObj.length--; callCount++; return a + b; }, 0); callCount", "2");
callCount = 0;
shouldBe("[1, 2, 3, 4].reduce(function(a,b, i, thisObj){ thisObj.length++; callCount++; return a + b; }, 0); callCount", "4");
callCount = 0;
shouldBe("toObject([1, 2, 3, 4]).reduce(function(a,b, i, thisObj){ thisObj.length--; callCount++; return a + b; }, 0); callCount", "4");
callCount = 0;
shouldBe("toObject([1, 2, 3, 4]).reduce(function(a,b, i, thisObj){ thisObj.length++; callCount++; return a + b; }, 0); callCount", "4");

shouldBe("[[0,1], [2,3], [4,5]].reduce(function(a,b) {return a.concat(b);}, [])", "[0,1,2,3,4,5]");
shouldBe("toObject([[0,1], [2,3], [4,5]]).reduce(function(a,b) {return a.concat(b);}, [])", "[0,1,2,3,4,5]");
shouldBe("toObject([0,1,2,3,4,5]).reduce(function(a,b,i) {return a.concat([i,b]);}, [])", "[0,0,1,1,2,2,3,3,4,4,5,5]");
shouldBe("toUnorderedObject([[0,1], [2,3], [4,5]]).reduce(function(a,b) {return a.concat(b);}, [])", "[0,1,2,3,4,5]");
shouldBe("toUnorderedObject([0,1,2,3,4,5]).reduce(function(a,b,i) {return a.concat([i,b]);}, [])", "[0,0,1,1,2,2,3,3,4,4,5,5]");
shouldBe("[0,1,2,3,4,5].reduce(function(a,b,i) {return a.concat([i,b]);}, [])", "[0,0,1,1,2,2,3,3,4,4,5,5]");
shouldBe("[2,3].reduce(function() {'use strict'; return this;})", "undefined");
