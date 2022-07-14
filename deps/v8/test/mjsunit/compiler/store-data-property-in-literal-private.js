// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --opt --trace-opt --trace-deopt
// Flags: --no-stress-flush-code --no-flush-bytecode
let privateSymbol = %CreatePrivateSymbol("private");
let privateName = %CreatePrivateNameSymbol("privateName");

function test() {
  "use strict";

  // These computed properties are translated into
  // JSDefineKeyedOwnPropertyInLiteral ops,
  // and AccessInfoFactory::ComputePropertyAccessInfo should find a
  // suitable map transition when optimizing. Even if the implementation details
  // are ignored, we still want to assert that these properties are installed as
  // non-enumerable, due to being private symbols.

  let obj = {
    [privateSymbol]: "private",
    [privateName]: "privateName",
  };

  assertPropertiesEqual(Object.getOwnPropertyDescriptor(obj, privateSymbol), {
    value: "private",
    writable: true,
    configurable: true,
    enumerable: false,
  });

  assertPropertiesEqual(Object.getOwnPropertyDescriptor(obj, privateName), {
    value: "privateName",
    writable: true,
    configurable: true,
    enumerable: false,
  });

  // They don't leak via Object.keys(), Object.getOwnPropertyNames, or
  // Object.getOwnPropertySymbols
  let props = Object.getOwnPropertyNames(obj);
  assertFalse(props.includes(privateSymbol));
  assertFalse(props.includes(privateName));

  let symbols = Object.getOwnPropertySymbols(obj);
  assertFalse(symbols.includes(privateSymbol));
  assertFalse(symbols.includes(privateName));

  let keys = Object.keys(obj);
  assertFalse(keys.includes(privateSymbol));
  assertFalse(keys.includes(privateName));

  return obj;
}

%PrepareFunctionForOptimization(test);
test();
test();
test();
%OptimizeFunctionOnNextCall(test);
test();
assertOptimized(test);
