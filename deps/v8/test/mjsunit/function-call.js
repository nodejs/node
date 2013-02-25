// Copyright 2011 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


var should_throw_on_null_and_undefined =
    [Object.prototype.toLocaleString,
     Object.prototype.valueOf,
     Object.prototype.hasOwnProperty,
     Object.prototype.isPrototypeOf,
     Object.prototype.propertyIsEnumerable,
     Array.prototype.concat,
     Array.prototype.join,
     Array.prototype.pop,
     Array.prototype.push,
     Array.prototype.reverse,
     Array.prototype.shift,
     Array.prototype.slice,
     Array.prototype.sort,
     Array.prototype.splice,
     Array.prototype.unshift,
     Array.prototype.indexOf,
     Array.prototype.lastIndexOf,
     Array.prototype.every,
     Array.prototype.some,
     Array.prototype.forEach,
     Array.prototype.map,
     Array.prototype.filter,
     Array.prototype.reduce,
     Array.prototype.reduceRight,
     String.prototype.charAt,
     String.prototype.charCodeAt,
     String.prototype.concat,
     String.prototype.indexOf,
     String.prototype.lastIndexOf,
     String.prototype.localeCompare,
     String.prototype.match,
     String.prototype.replace,
     String.prototype.search,
     String.prototype.slice,
     String.prototype.split,
     String.prototype.substring,
     String.prototype.toLowerCase,
     String.prototype.toLocaleLowerCase,
     String.prototype.toUpperCase,
     String.prototype.toLocaleUpperCase,
     String.prototype.trim,
     Number.prototype.toLocaleString];

// Non generic natives do not work on any input other than the specific
// type, but since this change will allow call to be invoked with undefined
// or null as this we still explicitly test that we throw on these here.
var non_generic =
    [Array.prototype.toString,
     Array.prototype.toLocaleString,
     Function.prototype.toString,
     Function.prototype.call,
     Function.prototype.apply,
     String.prototype.toString,
     String.prototype.valueOf,
     Boolean.prototype.toString,
     Boolean.prototype.valueOf,
     Number.prototype.toString,
     Number.prototype.valueOf,
     Number.prototype.toFixed,
     Number.prototype.toExponential,
     Number.prototype.toPrecision,
     Date.prototype.toString,
     Date.prototype.toDateString,
     Date.prototype.toTimeString,
     Date.prototype.toLocaleString,
     Date.prototype.toLocaleDateString,
     Date.prototype.toLocaleTimeString,
     Date.prototype.valueOf,
     Date.prototype.getTime,
     Date.prototype.getFullYear,
     Date.prototype.getUTCFullYear,
     Date.prototype.getMonth,
     Date.prototype.getUTCMonth,
     Date.prototype.getDate,
     Date.prototype.getUTCDate,
     Date.prototype.getDay,
     Date.prototype.getUTCDay,
     Date.prototype.getHours,
     Date.prototype.getUTCHours,
     Date.prototype.getMinutes,
     Date.prototype.getUTCMinutes,
     Date.prototype.getSeconds,
     Date.prototype.getUTCSeconds,
     Date.prototype.getMilliseconds,
     Date.prototype.getUTCMilliseconds,
     Date.prototype.getTimezoneOffset,
     Date.prototype.setTime,
     Date.prototype.setMilliseconds,
     Date.prototype.setUTCMilliseconds,
     Date.prototype.setSeconds,
     Date.prototype.setUTCSeconds,
     Date.prototype.setMinutes,
     Date.prototype.setUTCMinutes,
     Date.prototype.setHours,
     Date.prototype.setUTCHours,
     Date.prototype.setDate,
     Date.prototype.setUTCDate,
     Date.prototype.setMonth,
     Date.prototype.setUTCMonth,
     Date.prototype.setFullYear,
     Date.prototype.setUTCFullYear,
     Date.prototype.toUTCString,
     Date.prototype.toISOString,
     Date.prototype.toJSON,
     RegExp.prototype.exec,
     RegExp.prototype.test,
     RegExp.prototype.toString,
     Error.prototype.toString];


// Mapping functions.
var mapping_functions =
    [Array.prototype.every,
     Array.prototype.some,
     Array.prototype.forEach,
     Array.prototype.map,
     Array.prototype.filter];

// Reduce functions.
var reducing_functions =
    [Array.prototype.reduce,
     Array.prototype.reduceRight];

// Test that all natives using the ToObject call throw the right exception.
for (var i = 0; i < should_throw_on_null_and_undefined.length; i++) {
  // Sanity check that all functions are correct
  assertEquals(typeof(should_throw_on_null_and_undefined[i]), "function");

  var exception = false;
  try {
    // We call all functions with no parameters, which means that essential
    // parameters will have the undefined value.
    // The test for whether the "this" value is null or undefined is always
    // performed before access to the other parameters, so even if the
    // undefined value is an invalid argument value, it mustn't change
    // the result of the test.
    should_throw_on_null_and_undefined[i].call(null);
  } catch (e) {
    exception = true;
    assertTrue("called_on_null_or_undefined" == e.type ||
               "null_to_object" == e.type);
  }
  assertTrue(exception);

  exception = false;
  try {
    should_throw_on_null_and_undefined[i].call(undefined);
  } catch (e) {
    exception = true;
    assertTrue("called_on_null_or_undefined" == e.type ||
               "null_to_object" == e.type);
  }
  assertTrue(exception);

  exception = false;
  try {
    should_throw_on_null_and_undefined[i].apply(null);
  } catch (e) {
    exception = true;
    assertTrue("called_on_null_or_undefined" == e.type ||
               "null_to_object" == e.type);
  }
  assertTrue(exception);

  exception = false;
  try {
    should_throw_on_null_and_undefined[i].apply(undefined);
  } catch (e) {
    exception = true;
    assertTrue("called_on_null_or_undefined" == e.type ||
               "null_to_object" == e.type);
  }
  assertTrue(exception);
}

// Test that all natives that are non generic throw on null and undefined.
for (var i = 0; i < non_generic.length; i++) {
  // Sanity check that all functions are correct
  assertEquals(typeof(non_generic[i]), "function");

  exception = false;
  try {
    non_generic[i].call(null);
  } catch (e) {
    exception = true;
    assertTrue(e instanceof TypeError);
  }
  assertTrue(exception);

  exception = false;
  try {
    non_generic[i].call(null);
  } catch (e) {
    exception = true;
    assertTrue(e instanceof TypeError);
  }
  assertTrue(exception);

  exception = false;
  try {
    non_generic[i].apply(null);
  } catch (e) {
    exception = true;
    assertTrue(e instanceof TypeError);
  }
  assertTrue(exception);

  exception = false;
  try {
    non_generic[i].apply(null);
  } catch (e) {
    exception = true;
    assertTrue(e instanceof TypeError);
  }
  assertTrue(exception);
}


// Test that we still throw when calling with thisArg null or undefined
// through an array mapping function.
var array = [1,2,3,4,5];
for (var j = 0; j < mapping_functions.length; j++) {
  for (var i = 0; i < should_throw_on_null_and_undefined.length; i++) {
    exception = false;
    try {
      mapping_functions[j].call(array,
                                should_throw_on_null_and_undefined[i],
                                null);
    } catch (e) {
      exception = true;
      assertTrue("called_on_null_or_undefined" == e.type ||
                 "null_to_object" == e.type);
    }
    assertTrue(exception);

    exception = false;
    try {
      mapping_functions[j].call(array,
                                should_throw_on_null_and_undefined[i],
                                undefined);
    } catch (e) {
      exception = true;
      assertTrue("called_on_null_or_undefined" == e.type ||
                 "null_to_object" == e.type);
    }
    assertTrue(exception);
  }
}

for (var j = 0; j < mapping_functions.length; j++) {
  for (var i = 0; i < non_generic.length; i++) {
    exception = false;
    try {
      mapping_functions[j].call(array,
                                non_generic[i],
                                null);
    } catch (e) {
      exception = true;
      assertTrue(e instanceof TypeError);
    }
    assertTrue(exception);

    exception = false;
    try {
      mapping_functions[j].call(array,
                                non_generic[i],
                                undefined);
    } catch (e) {
      exception = true;
      assertTrue(e instanceof TypeError);
    }
    assertTrue(exception);
  }
}


// Reduce functions do a call with null as this argument.
for (var j = 0; j < reducing_functions.length; j++) {
  for (var i = 0; i < should_throw_on_null_and_undefined.length; i++) {
    exception = false;
    try {
      reducing_functions[j].call(array, should_throw_on_null_and_undefined[i]);
    } catch (e) {
      exception = true;
      assertTrue("called_on_null_or_undefined" == e.type ||
                 "null_to_object" == e.type);
    }
    assertTrue(exception);

    exception = false;
    try {
      reducing_functions[j].call(array, should_throw_on_null_and_undefined[i]);
    } catch (e) {
      exception = true;
      assertTrue("called_on_null_or_undefined" == e.type ||
                 "null_to_object" == e.type);
    }
    assertTrue(exception);
  }
}

for (var j = 0; j < reducing_functions.length; j++) {
  for (var i = 0; i < non_generic.length; i++) {
    exception = false;
    try {
      reducing_functions[j].call(array, non_generic[i]);
    } catch (e) {
      exception = true;
      assertTrue(e instanceof TypeError);
    }
    assertTrue(exception);

    exception = false;
    try {
      reducing_functions[j].call(array, non_generic[i]);
    } catch (e) {
      exception = true;
      assertTrue(e instanceof TypeError);
    }
    assertTrue(exception);
  }
}


// Object.prototype.toString()
assertEquals(Object.prototype.toString.call(null),
             '[object Null]')

assertEquals(Object.prototype.toString.call(undefined),
             '[object Undefined]')
