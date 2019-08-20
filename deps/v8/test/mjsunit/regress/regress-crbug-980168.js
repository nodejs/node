// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --verify-heap

// seal then freeze.
(function () {
  const v1 = Object.seal(Object);
  const v3 = Object();
  const v4 = Object(Object);
  v3.__proto__ = v4;
  const v6 = Object.freeze(Object);
})();

// preventExtensions then freeze.
(function () {
  const v1 = Object.preventExtensions(Object);
  const v3 = Object();
  const v4 = Object(Object);
  v3.__proto__ = v4;
  const v6 = Object.freeze(Object);
})();

// preventExtensions then seal.
(function () {
  const v1 = Object.preventExtensions(Object);
  const v3 = Object();
  const v4 = Object(Object);
  v3.__proto__ = v4;
  const v6 = Object.seal(Object);
})();

// freeze.
(function () {
  const v3 = Object();
  const v4 = Object(Object);
  v3.__proto__ = v4;
  const v6 = Object.freeze(Object);
})();

// seal.
(function () {
  const v3 = Object();
  const v4 = Object(Object);
  v3.__proto__ = v4;
  const v6 = Object.seal(Object);
})();

// preventExtensions.
(function () {
  const v3 = Object();
  const v4 = Object(Object);
  v3.__proto__ = v4;
  const v6 = Object.preventExtensions(Object);
})();
