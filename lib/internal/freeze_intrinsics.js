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
// SPDX-License-Identifier: MIT

// based upon:
// https://github.com/google/caja/blob/master/src/com/google/caja/ses/startSES.js
// https://github.com/google/caja/blob/master/src/com/google/caja/ses/repairES5.js
// https://github.com/tc39/proposal-frozen-realms/blob/91ac390e3451da92b5c27e354b39e52b7636a437/shim/src/deep-freeze.js

/* global WebAssembly, SharedArrayBuffer, console */
'use strict';
module.exports = function() {

  const intrinsics = [
    // Anonymous Intrinsics
    // ThrowTypeError
    Object.getOwnPropertyDescriptor(Function.prototype, 'caller').get,
    // IteratorPrototype
    Object.getPrototypeOf(
      Object.getPrototypeOf(new Array()[Symbol.iterator]())
    ),
    // ArrayIteratorPrototype
    Object.getPrototypeOf(new Array()[Symbol.iterator]()),
    // StringIteratorPrototype
    Object.getPrototypeOf(new String()[Symbol.iterator]()),
    // MapIteratorPrototype
    Object.getPrototypeOf(new Map()[Symbol.iterator]()),
    // SetIteratorPrototype
    Object.getPrototypeOf(new Set()[Symbol.iterator]()),
    // GeneratorFunction
    Object.getPrototypeOf(function* () {}),
    // AsyncFunction
    Object.getPrototypeOf(async function() {}),
    // AsyncGeneratorFunction
    Object.getPrototypeOf(async function* () {}),
    // TypedArray
    Object.getPrototypeOf(Uint8Array),

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

    // Disabled pending stack trace mutation handling
    // Error, // 19.5
    // EvalError,
    // RangeError,
    // ReferenceError,
    // SyntaxError,
    // TypeError,
    // URIError,

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

    // Web compatibility
    clearImmediate,
    clearInterval,
    clearTimeout,
    decodeURI,
    decodeURIComponent,
    encodeURI,
    encodeURIComponent,
    setImmediate,
    setInterval,
    setTimeout,

    // Other APIs
    console,
    BigInt,
    Atomics,
    WebAssembly,
    SharedArrayBuffer
  ];

  if (typeof Intl !== 'undefined')
    intrinsics.push(Intl);

  intrinsics.forEach(deepFreeze);

  function deepFreeze(root) {

    const { freeze, getOwnPropertyDescriptors, getPrototypeOf } = Object;
    const { ownKeys } = Reflect;

    // Objects that are deeply frozen.
    // It turns out that Error is reachable from WebAssembly so it is
    // explicitly added here to ensure it is not frozen
    const frozenSet = new WeakSet([Error, Error.prototype]);

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
          // todo use uncurried form
          // Ignore if already frozen or freezing
          return;
        }
        freezingSet.add(val); // todo use uncurried form
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
          // todo uncurried form
          // todo: getOwnPropertyDescriptors is guaranteed to return well-formed
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
        freezingSet.forEach(doFreeze); // todo curried forEach
      }

      function commit() {
        // todo curried forEach
        // we capture the real WeakSet.prototype.add above, in case someone
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
};
