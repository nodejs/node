// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags: --allow-natives-syntax

(function() {
  let obj = {};
  // Add a property
  Object.defineProperty(obj, "__proto__", { enumerable: true, writable: true });
  // Adds side-step from {__proto__} -> {}
  Object.assign({}, obj);
  // Generalizes {__proto__}
  Object.defineProperty(obj, "__proto__", { value: 0 });
})();

// TODO(olivf): Ensure object assign fastcase protects the object prototype.
// (function() {
//   var a = {};
//   a.x = 42;
//   Object.assign({}, a);
//   Object.defineProperty(Object.prototype, "x", {set(v) {}});
//   assertTrue(Object.assign({}, a).x === undefined);
// })();
