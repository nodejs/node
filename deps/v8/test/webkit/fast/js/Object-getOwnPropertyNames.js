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

description("Test to ensure correct behaviour of Object.getOwnPropertyNames");

function argumentsObject() { return arguments; };

var expectedPropertyNamesSet = {
    "{}": "[]",
    "{a:null}": "['a']",
    "{a:null, b:null}": "['a', 'b']",
    "{b:null, a:null}": "['a', 'b']",
    "{__proto__:{a:null}}": "[]",
    "{__proto__:[1,2,3]}": "[]",
    "Object.create({}, { 'a': { 'value': 1, 'enumerable': false } })": "['a']",
    "Object.create([1,2,3], { 'a': { 'value': 1, 'enumerable': false } })": "['a']",
// Function objects
    "new Function()": "['arguments', 'caller', 'length', 'name', 'prototype']",
    "(function(){var x=new Function();x.__proto__=[1,2,3];return x;})()": "['arguments', 'caller', 'length', 'name', 'prototype']",
// String objects
    "new String('')": "['length']",
    "new String('a')": "['0', 'length']",
    "new String('abc')": "['0', '1', '2', 'length']",
    "(function(){var x=new String('');x.__proto__=[1,2,3];return x;})()": "['length']",
// Array objects
    "[]": "['length']",
    "[null]": "['0', 'length']",
    "[null,null]": "['0','1', 'length']",
    "[null,null,,,,null]": "['0','1','5', 'length']",
    "(function(){var x=[];x.__proto__=[1,2,3];return x;})()": "['length']",
// Date objects
    "new Date()": "[]",
    "(function(){var x=new Date();x.__proto__=[1,2,3];return x;})()": "[]",
// RegExp objects
    "new RegExp('foo')": "['global', 'ignoreCase', 'lastIndex', 'multiline', 'source']",
    "(function(){var x=new RegExp();x.__proto__=[1,2,3];return x;})()": "['global', 'ignoreCase', 'lastIndex', 'multiline', 'source']",
// Arguments objects
     "argumentsObject()": "['callee', 'length']",
     "argumentsObject(1)": "['0', 'callee', 'length']",
     "argumentsObject(1,2,3)": "['0', '1', '2', 'callee', 'length']",
    "(function(){arguments.__proto__=[1,2,3];return arguments;})()": "['callee', 'length']",
// Built-in ECMA functions
    "parseInt": "['length', 'name']",
    "parseFloat": "['length', 'name']",
    "isNaN": "['length', 'name']",
    "isFinite": "['length', 'name']",
    "escape": "['length', 'name']",
    "unescape": "['length', 'name']",
    "decodeURI": "['length', 'name']",
    "decodeURIComponent": "['length', 'name']",
    "encodeURI": "['length', 'name']",
    "encodeURIComponent": "['length', 'name']",
// Built-in ECMA objects
    "Object": "['create', 'defineProperties', 'defineProperty', 'freeze', 'getOwnPropertyDescriptor', 'getOwnPropertyNames', 'getPrototypeOf', 'isExtensible', 'isFrozen', 'isSealed', 'keys', 'length', 'name', 'preventExtensions', 'prototype', 'seal', 'setPrototypeOf']",
    "Object.prototype": "['__defineGetter__', '__defineSetter__', '__lookupGetter__', '__lookupSetter__', '__proto__', 'constructor', 'hasOwnProperty', 'isPrototypeOf', 'propertyIsEnumerable', 'toLocaleString', 'toString', 'valueOf']",
    "Function": "['length', 'name', 'prototype']",
    "Function.prototype": "['apply', 'bind', 'call', 'constructor', 'length', 'name', 'toString']",
    "Array": "['isArray', 'length', 'name', 'prototype']",
    "Array.prototype": "['concat', 'constructor', 'every', 'filter', 'forEach', 'indexOf', 'join', 'lastIndexOf', 'length', 'map', 'pop', 'push', 'reduce', 'reduceRight', 'reverse', 'shift', 'slice', 'some', 'sort', 'splice', 'toLocaleString', 'toString', 'unshift']",
    "String": "['fromCharCode', 'length', 'name', 'prototype']",
    "String.prototype": "['anchor', 'big', 'blink', 'bold', 'charAt', 'charCodeAt', 'concat', 'constructor', 'fixed', 'fontcolor', 'fontsize', 'indexOf', 'italics', 'lastIndexOf', 'length', 'link', 'localeCompare', 'match', 'normalize', 'replace', 'search', 'slice', 'small', 'split', 'strike', 'sub', 'substr', 'substring', 'sup', 'toLocaleLowerCase', 'toLocaleUpperCase', 'toLowerCase', 'toString', 'toUpperCase', 'trim', 'trimLeft', 'trimRight', 'valueOf']",
    "Boolean": "['length', 'name', 'prototype']",
    "Boolean.prototype": "['constructor', 'toString', 'valueOf']",
    "Number": "['MAX_VALUE', 'MIN_VALUE', 'NEGATIVE_INFINITY', 'NaN', 'POSITIVE_INFINITY', 'length', 'name', 'prototype']",
    "Number.prototype": "['constructor', 'toExponential', 'toFixed', 'toLocaleString', 'toPrecision', 'toString', 'valueOf']",
    "Date": "['UTC', 'length', 'name', 'now', 'parse', 'prototype']",
    "Date.prototype": "['constructor', 'getDate', 'getDay', 'getFullYear', 'getHours', 'getMilliseconds', 'getMinutes', 'getMonth', 'getSeconds', 'getTime', 'getTimezoneOffset', 'getUTCDate', 'getUTCDay', 'getUTCFullYear', 'getUTCHours', 'getUTCMilliseconds', 'getUTCMinutes', 'getUTCMonth', 'getUTCSeconds', 'getYear', 'setDate', 'setFullYear', 'setHours', 'setMilliseconds', 'setMinutes', 'setMonth', 'setSeconds', 'setTime', 'setUTCDate', 'setUTCFullYear', 'setUTCHours', 'setUTCMilliseconds', 'setUTCMinutes', 'setUTCMonth', 'setUTCSeconds', 'setYear', 'toDateString', 'toGMTString', 'toISOString', 'toJSON', 'toLocaleDateString', 'toLocaleString', 'toLocaleTimeString', 'toString', 'toTimeString', 'toUTCString', 'valueOf']",
    "RegExp": "['$&', \"$'\", '$*', '$+', '$1', '$2', '$3', '$4', '$5', '$6', '$7', '$8', '$9', '$_', '$`', 'input', 'lastMatch', 'lastParen', 'leftContext', 'length', 'multiline', 'name', 'prototype', 'rightContext']",
    "RegExp.prototype": "['compile', 'constructor', 'exec', 'global', 'ignoreCase', 'lastIndex', 'multiline', 'source', 'test', 'toString']",
    "Error": "['length', 'name', 'prototype']",
    "Error.prototype": "['constructor', 'message', 'name', 'toString']",
    "Math": "['E', 'LN10', 'LN2', 'LOG10E', 'LOG2E', 'PI', 'SQRT1_2', 'SQRT2', 'abs', 'acos', 'asin', 'atan', 'atan2', 'ceil', 'cos', 'exp', 'floor', 'log', 'max', 'min', 'pow', 'random', 'round', 'sin', 'sqrt', 'tan']",
    "JSON": "['parse', 'stringify']"
};

function getSortedOwnPropertyNames(obj)
{
    return Object.getOwnPropertyNames(obj).sort();
}

for (var expr in expectedPropertyNamesSet)
    shouldBe("getSortedOwnPropertyNames(" + expr + ")", expectedPropertyNamesSet[expr]);

// Global Object
// Only check for ECMA properties here
var globalPropertyNames = Object.getOwnPropertyNames(this);
var expectedGlobalPropertyNames = [
    "NaN",
    "Infinity",
    "undefined",
    "parseInt",
    "parseFloat",
    "isNaN",
    "isFinite",
    "escape",
    "unescape",
    "decodeURI",
    "decodeURIComponent",
    "encodeURI",
    "encodeURIComponent",
    "Object",
    "Function",
    "Array",
    "String",
    "Boolean",
    "Number",
    "Date",
    "RegExp",
    "Error",
    "Math",
    "JSON"
];

for (var i = 0; i < expectedGlobalPropertyNames.length; ++i)
    shouldBeTrue("globalPropertyNames.indexOf('" + expectedGlobalPropertyNames[i] + "') != -1");
