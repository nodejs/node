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
"This test checks the behavior of the various array enumeration functions in certain edge case scenarios"
);

var functions = ["every", "forEach", "some", "filter", "reduce", "map", "reduceRight"];
var forwarders = [
    function(elem, index, array) { return currentFunc.call(this, elem, index, array); },
    function(elem, index, array) { return currentFunc.call(this, elem, index, array); },
    function(elem, index, array) { return currentFunc.call(this, elem, index, array); },
    function(elem, index, array) { return currentFunc.call(this, elem, index, array); },
    function(prev, elem, index, array) { return currentFunc.call(this, elem, index, array); },
    function(elem, index, array) { return currentFunc.call(this, elem, index, array); },
    function(prev, elem, index, array) { return currentFunc.call(this, elem, index, array); }
];

function toObject(array) {
    var o = {};
    for (var i in array)
        o[i] = array[i];
    o.length = array.length;
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
    return o;
}
function returnFalse() { count++; return false; }
function returnTrue() { count++; return true; }
function returnElem(elem) { count++; return elem; }
function returnIndex(a, index) { if (lastIndex >= index) throw "Unordered traversal"; lastIndex = index; count++; return index; }
function increaseLength(a, b, array) { count++; array.length++; }
function decreaseLength(a, b, array) { count++; array.length--; }
function halveLength(a, b, array) { count++; if (!array.halved) array.length = (array.length / 2) | 0; array.halved = true; }

var testFunctions = ["returnFalse", "returnTrue", "returnElem", "returnIndex", "increaseLength", "decreaseLength", "halveLength"];

var simpleArray = [0,1,2,3,4,5];
var emptyArray = [];
var largeEmptyArray = new Array(300);
var largeSparseArray = [0,1,2,3,4,5];
largeSparseArray[299] = 299;

var arrays = ["simpleArray", "emptyArray", "largeEmptyArray", "largeSparseArray"];
function copyArray(a) {
    var g = [];
    for (var i in a)
        g[i] = a[i];
    return g;
}

// Test object and array behaviour matches
for (var f = 0; f < functions.length; f++) {
    for (var t = 0; t < testFunctions.length; t++) {
        for (var a = 0; a < arrays.length; a++) {
            var functionName = functions[f];
            currentFunc = this[testFunctions[t]];
            if (arrays[a] === "largeEmptyArray" && functionName === "map")
                continue;
            if (currentFunc === returnIndex && functionName === "reduceRight")
                continue;
            shouldBe("count=0;lastIndex=-1;copyArray("+arrays[a]+")."+functionName+"(forwarders[f], "+testFunctions[t]+", 0)",
                     "count=0;lastIndex=-1;Array.prototype."+functionName+".call(toObject("+arrays[a]+"), forwarders[f], "+testFunctions[t]+", 0)");
        }
    }
}

// Test unordered object and array behaviour matches
for (var f = 0; f < functions.length; f++) {
    for (var t = 0; t < testFunctions.length; t++) {
        for (var a = 0; a < arrays.length; a++) {
            var functionName = functions[f];
            currentFunc = this[testFunctions[t]];
            if (arrays[a] === "largeEmptyArray" && functionName === "map")
                continue;
            if (currentFunc === returnIndex && functionName === "reduceRight")
                continue;
            shouldBe("count=0;lastIndex=-1;copyArray("+arrays[a]+")."+functionName+"(forwarders[f], "+testFunctions[t]+", 0)",
                     "count=0;lastIndex=-1;Array.prototype."+functionName+".call(toUnorderedObject("+arrays[a]+"), forwarders[f], "+testFunctions[t]+", 0)");
        }
    }
}

// Test number of function calls
var callCounts = [
[[1,0,0,1],[6,0,0,7],[1,0,0,1],[1,0,0,1],[1,0,0,1],[1,0,0,1],[1,0,0,1]],
[[6,0,0,7],[6,0,0,7],[6,0,0,7],[6,0,0,7],[6,0,0,7],[3,0,0,6],[3,0,0,6]],
[[6,0,0,7],[1,0,0,1],[2,0,0,2],[2,0,0,2],[6,0,0,7],[3,0,0,6],[3,0,0,6]],
[[6,0,0,7],[6,0,0,7],[6,0,0,7],[6,0,0,7],[6,0,0,7],[3,0,0,6],[3,0,0,6]],
[[6,0,0,7],[6,0,0,7],[6,0,0,7],[6,0,0,7],[6,0,0,7],[3,0,0,6],[3,0,0,6]],
[[6,0,0,7],[6,0,0,7],[6,0,0,7],[6,0,0,7],[6,0,0,7],[3,0,0,6],[3,0,0,6]],
[[6,0,0,7],[6,0,0,7],[6,0,0,7],[6,0,0,7],[6,0,0,7],[6,0,0,7],[4,0,0,7]]
];
var objCallCounts = [
[[1,0,0,1],[6,0,0,7],[1,0,0,1],[1,0,0,1],[1,0,0,1],[1,0,0,1],[1,0,0,1]],
[[6,0,0,7],[6,0,0,7],[6,0,0,7],[6,0,0,7],[6,0,0,7],[6,0,0,7],[6,0,0,7]],
[[6,0,0,7],[1,0,0,1],[2,0,0,2],[2,0,0,2],[6,0,0,7],[6,0,0,7],[6,0,0,7]],
[[6,0,0,7],[6,0,0,7],[6,0,0,7],[6,0,0,7],[6,0,0,7],[6,0,0,7],[6,0,0,7]],
[[6,0,0,7],[6,0,0,7],[6,0,0,7],[6,0,0,7],[6,0,0,7],[6,0,0,7],[6,0,0,7]],
[[6,0,0,7],[6,0,0,7],[6,0,0,7],[6,0,0,7],[6,0,0,7],[6,0,0,7],[6,0,0,7]],
[[6,0,0,7],[6,0,0,7],[6,0,0,7],[6,0,0,7],[6,0,0,7],[6,0,0,7],[6,0,0,7]]
];
for (var f = 0; f < functions.length; f++) {
    for (var t = 0; t < testFunctions.length; t++) {
        for (var a = 0; a < arrays.length; a++) {
            var functionName = functions[f];
            currentFunc = this[testFunctions[t]];
            if (currentFunc === returnIndex && functionName === "reduceRight")
                continue;
            var expectedCnt = "" + callCounts[f][t][a];
            shouldBe("count=0;lastIndex=-1;copyArray("+arrays[a]+")."+functionName+"(forwarders[f], "+testFunctions[t]+", 0); count", expectedCnt);
            var expectedCnt = "" + objCallCounts[f][t][a];
            shouldBe("count=0;lastIndex=-1;Array.prototype."+functionName+".call(toObject("+arrays[a]+"), forwarders[f], "+testFunctions[t]+", 0); count", expectedCnt);
            shouldBe("count=0;lastIndex=-1;Array.prototype."+functionName+".call(toObject("+arrays[a]+"), forwarders[f], "+testFunctions[t]+", 0); count", expectedCnt);
        }
    }
}
