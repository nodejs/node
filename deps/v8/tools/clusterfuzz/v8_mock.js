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
(function () {
  var index = 0
  Math.random = function() {
    index = (index + 1) % 10;
    return index / 10.0;
  }
})();

// Mock Date.
(function () {
  var index = 0
  var mockDate = 1477662728696
  var mockDateNow = function() {
    index = (index + 1) % 10
    mockDate = mockDate + index + 1
    return mockDate
  }

  var origDate = Date;
  var constructDate = function(args) {
    if (args.length == 1) {
      var result = new origDate(args[0]);
    } else if (args.length == 2) {
      var result = new origDate(args[0], args[1]);
    } else if (args.length == 3) {
      var result = new origDate(args[0], args[1], args[2]);
    } else if (args.length == 4) {
      var result = new origDate(args[0], args[1], args[2], args[3]);
    } else if (args.length == 5) {
      var result = new origDate(args[0], args[1], args[2], args[3], args[4]);
    } else if (args.length == 6) {
      var result = new origDate(
          args[0], args[1], args[2], args[3], args[4], args[5]);
    } else if (args.length >= 7) {
      var result = new origDate(
          args[0], args[1], args[2], args[3], args[4], args[5], args[6]);
    } else {
      var result = new origDate(mockDateNow());
    }
    result.constructor = function(...args) { return constructDate(args); }
    Object.defineProperty(
        result, "constructor", { configurable: false, writable: false });
    return result
  }

  var handler = {
    apply: function (target, thisArg, args) {
      return constructDate(args)
    },
    construct: function (target, args, newTarget) {
      return constructDate(args)
    },
    get: function(target, property, receiver) {
      if (property == "now") {
        return mockDateNow;
      }
      if (property == "prototype") {
        return origDate.prototype
      }
    },
  }

  Date = new Proxy(Date, handler);
})();

// Mock performace methods.
(function () {
  performance.now = function () { return 1.2; }
  performance.measureMemory = function () { return []; }
})();

// Mock stack traces.
Error.prepareStackTrace = function (error, structuredStackTrace) {
  return "";
};
Object.defineProperty(
    Error, 'prepareStackTrace', { configurable: false, writable: false });

// Mock buffer access in float typed arrays because of varying NaN patterns.
// Note, for now we just use noop forwarding proxies, because they already
// turn off optimizations.
(function () {
  var mock = function(arrayType) {
    var handler = {
      construct: function(target, args) {
        var obj = new (Function.prototype.bind.apply(arrayType, [null].concat(args)));
        return new Proxy(obj, {
          get: function(x, prop) {
            if (typeof x[prop] == "function")
              return x[prop].bind(obj)
            return x[prop];
          },
        });
      },
    };
    return new Proxy(arrayType, handler);
  }

  Float32Array = mock(Float32Array);
  Float64Array = mock(Float64Array);
})();

// Mock Worker.
(function () {
  var index = 0;
  // TODO(machenbach): Randomize this for each test case, but keep stable
  // during comparison. Also data and random above.
  var workerMessages = [
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
