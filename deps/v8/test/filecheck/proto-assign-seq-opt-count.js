// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --proto-assign-seq-opt --proto-assign-seq-opt-count=3
// Flags: --print-bytecode --print-bytecode-filter=assignments*
// Flags: --no-stress-lazy-source-positions --no-stress-background-compile

(function assignments2() {
  function f(){}
  f.prototype.a = 1;
  f.prototype.b = 2;
  return new f();
})();

(function assignments3() {
  function f(){}
  f.prototype.a = 1;
  f.prototype.b = 2;
  f.prototype.c = 3;
  return new f();
})();

(function assignments4() {
  function f(){}
  f.prototype.a = 1;
  f.prototype.b = 2;
  f.prototype.c = 3;
  f.prototype.d = 4;
  return new f();
})();

// CHECK-LABEL: assignments4
// CHECK-NOT: SetNamedProperty
// CHECK: SetPrototypeProperties
// CHECK-NOT: SetNamedProperty

// CHECK-LABEL: assignments3
// CHECK-NOT: SetNamedProperty
// CHECK: SetPrototypeProperties
// CHECK-NOT: SetNamedProperty

// CHECK-LABEL: assignments2
// CHECK-NOT: SetPrototypeProperties
// CHECK: SetNamedProperty
// CHECK-NOT: SetPrototypeProperties
// CHECK: SetNamedProperty
// CHECK-NOT: SetPrototypeProperties
