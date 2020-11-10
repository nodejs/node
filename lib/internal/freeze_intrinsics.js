// Adapted from SES/Caja - Copyright (C) 2011 Google Inc.
// Copyright (C) 2018 Agoric

// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// SPDX-License-Identifier: Apache-2.0

// Based upon:
// https://github.com/google/caja/blob/master/src/com/google/caja/ses/startSES.js
// https://github.com/google/caja/blob/master/src/com/google/caja/ses/repairES5.js
// https://github.com/tc39/proposal-ses/blob/e5271cc42a257a05dcae2fd94713ed2f46c08620/shim/src/freeze.js

/* global WebAssembly, SharedArrayBuffer, console */
/* eslint-disable no-restricted-globals */
'use strict';

module.exports = function() {
  const {
    defineProperty,
    freeze,
    getOwnPropertyDescriptor,
    getOwnPropertyDescriptors,
    getOwnPropertyNames,
    getOwnPropertySymbols,
    getPrototypeOf,
  } = Object;
  const objectHasOwnProperty = Object.prototype.hasOwnProperty;
  const { ownKeys } = Reflect;
  const {
    clearImmediate,
    clearInterval,
    clearTimeout,
    setImmediate,
    setInterval,
    setTimeout
  } = require('timers');

  const intrinsicPrototypes = [
    // Anonymous Intrinsics
    // IteratorPrototype
    getPrototypeOf(
      getPrototypeOf(new Array()[Symbol.iterator]())
    ),
    // ArrayIteratorPrototype
    getPrototypeOf(new Array()[Symbol.iterator]()),
    // StringIteratorPrototype
    getPrototypeOf(new String()[Symbol.iterator]()),
    // MapIteratorPrototype
    getPrototypeOf(new Map()[Symbol.iterator]()),
    // SetIteratorPrototype
    getPrototypeOf(new Set()[Symbol.iterator]()),
    // GeneratorFunction
    getPrototypeOf(function* () {}),
    // AsyncFunction
    getPrototypeOf(async function() {}),
    // AsyncGeneratorFunction
    getPrototypeOf(async function* () {}),
    // TypedArray
    getPrototypeOf(Uint8Array),

    // 19 Fundamental Objects
    Object.prototype, // 19.1
    Function.prototype, // 19.2
    Boolean.prototype, // 19.3

    Error.prototype, // 19.5
    EvalError.prototype,
    RangeError.prototype,
    ReferenceError.prototype,
    SyntaxError.prototype,
    TypeError.prototype,
    URIError.prototype,

    // 20 Numbers and Dates
    Number.prototype, // 20.1
    Date.prototype, // 20.3

    // 21 Text Processing
    String.prototype, // 21.1
    RegExp.prototype, // 21.2

    // 22 Indexed Collections
    Array.prototype, // 22.1

    Int8Array.prototype,
    Uint8Array.prototype,
    Uint8ClampedArray.prototype,
    Int16Array.prototype,
    Uint16Array.prototype,
    Int32Array.prototype,
    Uint32Array.prototype,
    Float32Array.prototype,
    Float64Array.prototype,
    BigInt64Array.prototype,
    BigUint64Array.prototype,

    // 23 Keyed Collections
    Map.prototype, // 23.1
    Set.prototype, // 23.2
    WeakMap.prototype, // 23.3
    WeakSet.prototype, // 23.4

    // 24 Structured Data
    ArrayBuffer.prototype, // 24.1
    DataView.prototype, // 24.3
    Promise.prototype, // 25.4

    // Other APIs / Web Compatibility
    console.Console.prototype,
    BigInt.prototype,
    WebAssembly.Module.prototype,
    WebAssembly.Instance.prototype,
    WebAssembly.Table.prototype,
    WebAssembly.Memory.prototype,
    WebAssembly.CompileError.prototype,
    WebAssembly.LinkError.prototype,
    WebAssembly.RuntimeError.prototype,
    SharedArrayBuffer.prototype
  ];
  const intrinsics = [
    // Anonymous Intrinsics
    // ThrowTypeError
    getOwnPropertyDescriptor(Function.prototype, 'caller').get,
    // IteratorPrototype
    getPrototypeOf(
      getPrototypeOf(new Array()[Symbol.iterator]())
    ),
    // ArrayIteratorPrototype
    getPrototypeOf(new Array()[Symbol.iterator]()),
    // StringIteratorPrototype
    getPrototypeOf(new String()[Symbol.iterator]()),
    // MapIteratorPrototype
    getPrototypeOf(new Map()[Symbol.iterator]()),
    // SetIteratorPrototype
    getPrototypeOf(new Set()[Symbol.iterator]()),
    // GeneratorFunction
    getPrototypeOf(function* () {}),
    // AsyncFunction
    getPrototypeOf(async function() {}),
    // AsyncGeneratorFunction
    getPrototypeOf(async function* () {}),
    // TypedArray
    getPrototypeOf(Uint8Array),

    // 18 The Global Object
    eval,
    isFinite,
    isNaN,
    parseFloat,
    parseInt,
    decodeURI,
    decodeURIComponent,
    encodeURI,
    encodeURIComponent,

    // 19 Fundamental Objects
    Object, // 19.1
    Function, // 19.2
    Boolean, // 19.3
    Symbol, // 19.4

    Error, // 19.5
    EvalError,
    RangeError,
    ReferenceError,
    SyntaxError,
    TypeError,
    URIError,

    // 20 Numbers and Dates
    Number, // 20.1
    Math, // 20.2
    Date, // 20.3

    // 21 Text Processing
    String, // 21.1
    RegExp, // 21.2

    // 22 Indexed Collections
    Array, // 22.1

    Int8Array,
    Uint8Array,
    Uint8ClampedArray,
    Int16Array,
    Uint16Array,
    Int32Array,
    Uint32Array,
    Float32Array,
    Float64Array,
    BigInt64Array,
    BigUint64Array,

    // 23 Keyed Collections
    Map, // 23.1
    Set, // 23.2
    WeakMap, // 23.3
    WeakSet, // 23.4

    // 24 Structured Data
    ArrayBuffer, // 24.1
    DataView, // 24.3
    JSON, // 24.5
    Promise, // 25.4

    // 26 Reflection
    Reflect, // 26.1
    Proxy, // 26.2

    // B.2.1
    escape,
    unescape,

    // Other APIs / Web Compatibility
    clearImmediate,
    clearInterval,
    clearTimeout,
    setImmediate,
    setInterval,
    setTimeout,
    console,
    BigInt,
    Atomics,
    WebAssembly,
    SharedArrayBuffer
  ];

  if (typeof Intl !== 'undefined') {
    intrinsicPrototypes.push(Intl.Collator.prototype);
    intrinsicPrototypes.push(Intl.DateTimeFormat.prototype);
    intrinsicPrototypes.push(Intl.ListFormat.prototype);
    intrinsicPrototypes.push(Intl.NumberFormat.prototype);
    intrinsicPrototypes.push(Intl.PluralRules.prototype);
    intrinsicPrototypes.push(Intl.RelativeTimeFormat.prototype);
    intrinsics.push(Intl);
  }

  intrinsicPrototypes.forEach(enableDerivedOverrides);

  const frozenSet = new WeakSet();
  intrinsics.forEach(deepFreeze);

  // Objects that are deeply frozen.
  function deepFreeze(root) {
    /**
     * "innerDeepFreeze()" acts like "Object.freeze()", except that:
     *
     * To deepFreeze an object is to freeze it and all objects transitively
     * reachable from it via transitive reflective property and prototype
     * traversal.
     */
    function innerDeepFreeze(node) {
      // Objects that we have frozen in this round.
      const freezingSet = new Set();

      // If val is something we should be freezing but aren't yet,
      // add it to freezingSet.
      function enqueue(val) {
        if (Object(val) !== val) {
          // ignore primitives
          return;
        }
        const type = typeof val;
        if (type !== 'object' && type !== 'function') {
          // NB: handle for any new cases in future
        }
        if (frozenSet.has(val) || freezingSet.has(val)) {
          // TODO: Use uncurried form
          // Ignore if already frozen or freezing
          return;
        }
        freezingSet.add(val); // TODO: Use uncurried form
      }

      function doFreeze(obj) {
        // Immediately freeze the object to ensure reactive
        // objects such as proxies won't add properties
        // during traversal, before they get frozen.

        // Object are verified before being enqueued,
        // therefore this is a valid candidate.
        // Throws if this fails (strict mode).
        freeze(obj);

        // We rely upon certain commitments of Object.freeze and proxies here

        // Get stable/immutable outbound links before a Proxy has a chance to do
        // something sneaky.
        const proto = getPrototypeOf(obj);
        const descs = getOwnPropertyDescriptors(obj);
        enqueue(proto);
        ownKeys(descs).forEach((name) => {
          // TODO: Uncurried form
          // TODO: getOwnPropertyDescriptors is guaranteed to return well-formed
          // descriptors, but they still inherit from Object.prototype. If
          // someone has poisoned Object.prototype to add 'value' or 'get'
          // properties, then a simple 'if ("value" in desc)' or 'desc.value'
          // test could be confused. We use hasOwnProperty to be sure about
          // whether 'value' is present or not, which tells us for sure that
          // this is a data property.
          const desc = descs[name];
          if ('value' in desc) {
            // todo uncurried form
            enqueue(desc.value);
          } else {
            enqueue(desc.get);
            enqueue(desc.set);
          }
        });
      }

      function dequeue() {
        // New values added before forEach() has finished will be visited.
        freezingSet.forEach(doFreeze); // TODO: Curried forEach
      }

      function commit() {
        // TODO: Curried forEach
        // We capture the real WeakSet.prototype.add above, in case someone
        // changes it. The two-argument form of forEach passes the second
        // argument as the 'this' binding, so we add to the correct set.
        freezingSet.forEach(frozenSet.add, frozenSet);
      }

      enqueue(node);
      dequeue();
      commit();
    }

    innerDeepFreeze(root);
    return root;
  }

  /**
   * For a special set of properties (defined below), it ensures that the
   * effect of freezing does not suppress the ability to override these
   * properties on derived objects by simple assignment.
   *
   * Because of lack of sufficient foresight at the time, ES5 unfortunately
   * specified that a simple assignment to a non-existent property must fail if
   * it would override a non-writable data property of the same name. (In
   * retrospect, this was a mistake, but it is now too late and we must live
   * with the consequences.) As a result, simply freezing an object to make it
   * tamper proof has the unfortunate side effect of breaking previously correct
   * code that is considered to have followed JS best practices, if this
   * previous code used assignment to override.
   *
   * To work around this mistake, deepFreeze(), prior to freezing, replaces
   * selected configurable own data properties with accessor properties which
   * simulate what we should have specified -- that assignments to derived
   * objects succeed if otherwise possible.
   */
  function enableDerivedOverride(obj, prop, desc) {
    if ('value' in desc && desc.configurable) {
      const value = desc.value;

      function getter() {
        return value;
      }

      // Re-attach the data property on the object so
      // it can be found by the deep-freeze traversal process.
      getter.value = value;

      function setter(newValue) {
        if (obj === this) {
          // eslint-disable-next-line no-restricted-syntax
          throw new TypeError(
            `Cannot assign to read only property '${prop}' of object '${obj}'`
          );
        }
        if (objectHasOwnProperty.call(this, prop)) {
          this[prop] = newValue;
        } else {
          defineProperty(this, prop, {
            value: newValue,
            writable: true,
            enumerable: desc.enumerable,
            configurable: desc.configurable
          });
        }
      }

      defineProperty(obj, prop, {
        get: getter,
        set: setter,
        enumerable: desc.enumerable,
        configurable: desc.configurable
      });
    }
  }

  function enableDerivedOverrides(obj) {
    if (!obj) {
      return;
    }
    const descs = getOwnPropertyDescriptors(obj);
    if (!descs) {
      return;
    }
    getOwnPropertyNames(obj).forEach((prop) => {
      return enableDerivedOverride(obj, prop, descs[prop]);
    });
    getOwnPropertySymbols(obj).forEach((prop) => {
      return enableDerivedOverride(obj, prop, descs[prop]);
    });
  }
};
