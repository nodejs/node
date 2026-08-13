// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function() {
  const origFill = Uint16Array.prototype.fill;
  const mock = function(type) {
    type.prototype.fill = function(value, start, end) {
      origFill.apply(this, [value, start, end]);
    };
    const handler = {
      construct: function(target, args) {
        const underlying_obj = new type(...args);
        return new Proxy(underlying_obj, {
          get: function(x, prop) {
            if (typeof x[prop] == "function") return x[prop].bind(underlying_obj);
            return x[prop];
          }
        });
      },
    };
    return new Proxy(type, handler);
  }
  Uint16Array = mock(Uint16Array);
})();

function create_fun() {
  const arr = new Uint16Array(1);
  arr.fill(3);
  return new Uint16Array(arr.buffer)[0];
}

%PrepareFunctionForOptimization(create_fun);
assertEquals(3, create_fun());
%OptimizeFunctionOnNextCall(create_fun);
assertEquals(3, create_fun());
