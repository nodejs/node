// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function returnFalsishStrict() {
  "use strict";

  function trySet(o) {
    o["bla"] = 0;
  }

  var proxy = new Proxy({}, {});
  var proxy2 = new Proxy({}, { set() { return ""; } });

  trySet(proxy);
  trySet(proxy);
  assertThrows(() => trySet(proxy2), TypeError);
})();

(function privateSymbolStrict() {
  "use strict";
  var proxy = new Proxy({}, {});
  var proxy2 = new Proxy({a: 1}, { set() { return true; } });

  function trySet(o) {
    var symbol = o == proxy2 ? %CreatePrivateSymbol("private"): 1;
    o[symbol] = 0;
  }

  trySet(proxy);
  trySet(proxy);
  assertThrows(() => trySet(proxy2), TypeError);
})();
