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
  let index = 0
  Math.random = function() {
    index = (index + 1) % 10;
    return index / 10.0;
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
  const origIsNaN = isNaN;
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
              typeof args[i][Symbol.iterator] === 'function') {
            // Consume iterators.
            args[i] = Array.from(args[i]);
          }
          if (Array.isArray(args[i])) {
            args[i] = args[i].map(deNaNify);
          }
        }

        const obj = new (
            Function.prototype.bind.call(type, null, ...args));
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
