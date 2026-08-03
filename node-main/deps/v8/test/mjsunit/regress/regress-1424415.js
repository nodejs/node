// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// This disables inline allocation.
%SetAllocationTimeout(20, 0, false);
// Allocate and eventually trigger optimization.
for (let i = 0; i < 10000; ++i) {
  function f() { return i; }
}
// This should not crash!
%SimulateNewspaceFull();
