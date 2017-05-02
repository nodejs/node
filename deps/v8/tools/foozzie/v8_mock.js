// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is intended for permanent JS behavior changes for mocking out
// non-deterministic behavior. For temporary suppressions, please refer to
// v8_suppressions.js.
// This file is loaded before each correctness test cases and won't get
// minimized.


// This will be overridden in the test cases. The override can be minimized.
var __PrettyPrint = function __PrettyPrint(msg) { print(msg); };

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
  var handler = {
    construct: function(target, args, newTarget) {
      if (args.length > 0) {
        return new (
            Function.prototype.bind.apply(origDate, [null].concat(args)));
      } else {
        return new origDate(mockDateNow());
      }
    },
    get: function(target, property, receiver) {
      if (property == "now") {
        return mockDateNow;
      }
    },
  }

  Date = new Proxy(Date, handler);
})();

// Mock performace.now().
(function () {
  performance.now = function () { return 1.2; }
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
        return new Proxy(
            Function.prototype.bind.apply(arrayType, [null].concat(args)), {});
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
      __PrettyPrint(eval(code));
    } catch(e) {
      __PrettyPrint(e);
    }
    this.getMessage = function(){
      index = (index + 1) % 10;
      return workerMessages[index];
    }
    this.postMessage = function(msg){
      __PrettyPrint(msg);
    }
  };
})();
