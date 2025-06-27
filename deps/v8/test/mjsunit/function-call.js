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


const should_throw_on_null_and_undefined =
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
     String.prototype.trim];

// Non generic natives do not work on any input other than the specific
// type, but since this change will allow call to be invoked with undefined
// or null as this we still explicitly test that we throw on these here.
const non_generic =
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
const mapping_functions =
    [Array.prototype.every,
     Array.prototype.some,
     Array.prototype.forEach,
     Array.prototype.map,
     Array.prototype.filter];

// Reduce functions.
const reducing_functions =
    [Array.prototype.reduce,
     Array.prototype.reduceRight];

function checkExpectedMessage(e) {
  assertTrue(e.message.includes("called on null or undefined") ||
      e.message.includes("invoked on undefined or null value") ||
      e.message.includes("Cannot convert undefined or null to object"));
}

// Test that all natives using the ToObject call throw the right exception.
for (const fn of should_throw_on_null_and_undefined) {
  // Sanity check that all functions are correct
  assertEquals(typeof fn, "function");

  let exception = false;
  try {
    // We need to pass a dummy object argument ({}) to these functions because
    // of Object.prototype.isPrototypeOf's special behavior, see issue 3483
    // for more details.
    fn.call(null, {});
  } catch (e) {
    exception = true;
    checkExpectedMessage(e);
  }
  assertTrue(exception);

  exception = false;
  try {
    fn.call(undefined, {});
  } catch (e) {
    exception = true;
    checkExpectedMessage(e);
  }
  assertTrue(exception);

  exception = false;
  try {
    fn.apply(null, [{}]);
  } catch (e) {
    exception = true;
    checkExpectedMessage(e);
  }
  assertTrue(exception);

  exception = false;
  try {
    fn.apply(undefined, [{}]);
  } catch (e) {
    exception = true;
    checkExpectedMessage(e);
  }
  assertTrue(exception);
}

// Test that all natives that are non generic throw on null and undefined.
for (const fn of non_generic) {
  // Sanity check that all functions are correct
  assertEquals(typeof fn, "function");

  exception = false;
  try {
    fn.call(null);
  } catch (e) {
    exception = true;
    assertTrue(e instanceof TypeError);
  }
  assertTrue(exception);

  exception = false;
  try {
    fn.call(null);
  } catch (e) {
    exception = true;
    assertTrue(e instanceof TypeError);
  }
  assertTrue(exception);

  exception = false;
  try {
    fn.apply(null);
  } catch (e) {
    exception = true;
    assertTrue(e instanceof TypeError);
  }
  assertTrue(exception);

  exception = false;
  try {
    fn.apply(null);
  } catch (e) {
    exception = true;
    assertTrue(e instanceof TypeError);
  }
  assertTrue(exception);
}


// Test that we still throw when calling with thisArg null or undefined
// through an array mapping function.
// We need to make sure that the elements of `array` are all object values,
// see issue 3483 for more details.
const array = [{}, [], new Number, new Map, new WeakSet];
for (const mapping_function of mapping_functions) {
  for (const fn of should_throw_on_null_and_undefined) {
    exception = false;
    try {
      mapping_function.call(array,
                            fn,
                            null);
    } catch (e) {
      exception = true;
      checkExpectedMessage(e);
    }
    assertTrue(exception);

    exception = false;
    try {
      mapping_function.call(array,
                            fn,
                            undefined);
    } catch (e) {
      exception = true;
      checkExpectedMessage(e);
    }
    assertTrue(exception);
  }
}

for (const mapping_function of mapping_functions) {
  for (const fn of non_generic) {
    exception = false;
    try {
      mapping_function.call(array,
                            fn,
                            null);
    } catch (e) {
      exception = true;
      assertTrue(e instanceof TypeError);
    }
    assertTrue(exception);

    exception = false;
    try {
      mapping_function.call(array,
                            fn,
                            undefined);
    } catch (e) {
      exception = true;
      assertTrue(e instanceof TypeError);
    }
    assertTrue(exception);
  }
}


// Reduce functions do a call with null as this argument.
for (const reducing_function of reducing_functions) {
  for (const fn of should_throw_on_null_and_undefined) {
    exception = false;
    try {
      reducing_function.call(array, fn);
    } catch (e) {
      exception = true;
      checkExpectedMessage(e);
    }
    assertTrue(exception);

    exception = false;
    try {
      reducing_function.call(array, fn);
    } catch (e) {
      exception = true;
      checkExpectedMessage(e);
    }
    assertTrue(exception);
  }
}

for (const reducing_function of reducing_functions) {
  for (const fn of non_generic) {
    exception = false;
    try {
      reducing_function.call(array, fn);
    } catch (e) {
      exception = true;
      assertTrue(e instanceof TypeError);
    }
    assertTrue(exception);

    exception = false;
    try {
      reducing_function.call(array, fn);
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
