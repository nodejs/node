// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is intended for permanent JS behavior changes for mocking out
// non-deterministic behavior. For temporary suppressions, please refer to
// v8_suppressions.js.
// This file is loaded before each correctness test cases and won't get
// minimized.


// This will be overridden in the test cases. The override can be minimized.
var prettyPrinted = function prettyPrinted(msg) { return msg; };

// Mock Math.random.
(function() {
  let index = 1;
  Math.random = function() {
      const x = Math.sin(index++) * 10000;
      return x - Math.floor(x);
  }
})();

// Mock Math.pow. Work around an optimization for -0.5.
(function() {
  const origMathPow = Math.pow;
  Math.pow = function(a, b) {
    if (b === -0.5) {
      return 0;
    } else {
      return origMathPow(a, b);
    }
  }
})();


// Mock Date.
(function() {
  let index = 0;
  let mockDate = 1477662728696;
  const mockDateNow = function() {
    index = (index + 1) % 10;
    mockDate = mockDate + index + 1;
    return mockDate;
  }

  const origDate = Date;
  const construct = Reflect.construct;
  const constructDate = function(args) {
    let result;
    if (args.length) {
      result = construct(origDate, args);
    } else {
      result = new origDate(mockDateNow());
    }
    result.constructor = function(...args) { return constructDate(args); }
    Object.defineProperty(
        result, "constructor", { configurable: false, writable: false });
    return result;
  }

  origDate.prototype.constructor = function(...args) {
    return constructDate(args);
  };

  var handler = {
    apply: function(target, thisArg, args) {
      return constructDate(args);
    },
    construct: function(target, args, newTarget) {
      return constructDate(args);
    },
    get: function(target, property, receiver) {
      if (property == "now") {
        return mockDateNow;
      }
      if (property == "prototype") {
        return origDate.prototype;
      }
    },
  }

  Date = new Proxy(Date, handler);
})();

// Mock performance methods.
performance.now = function() { return 1.2; };
performance.measureMemory = function() { return []; };

// Mock readline so that test cases don't hang.
readline = function() { return "foo"; };

// Mock stack traces.
Error.prepareStackTrace = function(error, structuredStackTrace) {
  return "";
};
Object.defineProperty(
    Error, 'prepareStackTrace', { configurable: false, writable: false });

// Mock buffer access in float typed arrays because of varying NaN patterns.
(function() {
  const origArrayFrom = Array.from;
  const origArrayIsArray = Array.isArray;
  const origFunctionPrototype = Function.prototype;
  const origIsNaN = isNaN;
  const origIterator = Symbol.iterator;
  const deNaNify = function(value) { return origIsNaN(value) ? 1 : value; };
  const mock = function(type) {

    // Remove NaN values from parameters to "set" function.
    const set = type.prototype.set;
    type.prototype.set = function(array, offset) {
      if (Array.isArray(array)) {
        array = array.map(deNaNify);
      }
      set.apply(this, [array, offset]);
    };

    const handler = {
      // Remove NaN values from parameters to constructor.
      construct: function(target, args) {
        for (let i = 0; i < args.length; i++) {
          if (args[i] != null &&
              typeof args[i][origIterator] === 'function') {
            // Consume iterators.
            args[i] = origArrayFrom(args[i]);
          }
          if (origArrayIsArray(args[i])) {
            args[i] = args[i].map(deNaNify);
          }
        }

        const obj = new (
            origFunctionPrototype.bind.call(type, null, ...args));
        return new Proxy(obj, {
          get: function(x, prop) {
            if (typeof x[prop] == "function")
              return x[prop].bind(obj);
            return x[prop];
          },
          // Remove NaN values that get assigned.
          set: function(target, prop, value, receiver) {
            target[prop] = deNaNify(value);
            return value;
          }
        });
      },
    };
    return new Proxy(type, handler);
  }

  Float32Array = mock(Float32Array);
  Float64Array = mock(Float64Array);
})();

// Mock buffer access via DataViews because of varying NaN patterns.
(function() {
  const origIsNaN = isNaN;
  const deNaNify = function(value) { return origIsNaN(value) ? 1 : value; };
  const origSetFloat32 = DataView.prototype.setFloat32;
  DataView.prototype.setFloat32 = function(offset, value, ...rest) {
    origSetFloat32.call(this, offset, deNaNify(value), ...rest);
  };
  const origSetFloat64 = DataView.prototype.setFloat64;
  DataView.prototype.setFloat64 = function(offset, value, ...rest) {
    origSetFloat64.call(this, offset, deNaNify(value), ...rest);
  };
})();

// Mock Worker.
(function() {
  let index = 0;
  // TODO(machenbach): Randomize this for each test case, but keep stable
  // during comparison. Also data and random above.
  const workerMessages = [
    undefined, 0, -1, "", "foo", 42, [], {}, [0], {"x": 0}
  ];
  Worker = function(code){
    try {
      print(prettyPrinted(eval(code)));
    } catch(e) {
      print(prettyPrinted(e));
    }
    this.getMessage = function(){
      index = (index + 1) % 10;
      return workerMessages[index];
    }
    this.postMessage = function(msg){
      print(prettyPrinted(msg));
    }
  };
})();

// Mock Realm.
Realm.eval = function(realm, code) { return eval(code) };

// Mock the nondeterministic parts of WeakRef and FinalizationRegistry.
WeakRef.prototype.deref = function() { };
FinalizationRegistry = function(callback) { };
FinalizationRegistry.prototype.register = function(target, holdings) { };
FinalizationRegistry.prototype.unregister = function(unregisterToken) { };
FinalizationRegistry.prototype.cleanupSome = function() { };
FinalizationRegistry.prototype[Symbol.toStringTag] = "FinalizationRegistry";

// Mock the nondeterministic Atomics.waitAsync.
Atomics.waitAsync = function() {
  // Return a mock "Promise" whose "then" function will call the callback
  // immediately.
  return {'value': {'then': function (f) { f(); }}};
}
