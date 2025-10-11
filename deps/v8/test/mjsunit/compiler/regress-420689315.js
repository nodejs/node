// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

function executeCode(code) {
  if (typeof code === 'function') return code();
};
assertThrows = function assertThrows(code) {
  try {
    executeCode(code);
  } catch (e) {
  }
};
function __f_9() {
  Reflect.construct();
  Reflect.construct(23, arguments);
  Reflect.construct(23, arguments);
}
function __f_10() {
  return new __f_9();
}
%PrepareFunctionForOptimization(__f_10);
assertThrows(__f_10);
%OptimizeFunctionOnNextCall(__f_10);
assertThrows(__f_10);
