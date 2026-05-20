// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Some more examples to test multiple input files.

// Contains statements that are extracted:

function foo() {
  // Logical.
  true || false;
}

// Statements that are not extracted:

function bar() {
  unknown();
}
