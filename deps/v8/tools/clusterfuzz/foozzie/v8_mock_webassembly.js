// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This mocks out the WebAssembly object with a permissive dummy.

(function() {
  const handler = {
    get: function(x, prop) {
      if (prop == Symbol.toPrimitive) {
        return function() { return undefined; };
      }
      return dummy;
    },
  };
  const dummy = new Proxy(function() { return dummy; }, handler);
  WebAssembly = dummy;
})();
