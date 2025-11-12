// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

assertThrows(
  // Should throw a syntax error, but not crash.
  "({ __proto__: null, __proto__: 1 })",
  SyntaxError
);
