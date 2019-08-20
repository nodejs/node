// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm --experimental-wasm-anyref
// Flags: --wasm-interpret-all

// This is just a wrapper for an existing reference types test case that runs
// with the --wasm-interpret-all flag added. If we ever decide to add a test
// variant for this, then we can remove this file.

load("test/mjsunit/wasm/table-grow-from-wasm.js");
