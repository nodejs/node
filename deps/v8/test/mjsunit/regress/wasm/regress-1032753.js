// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --perf-prof --perf-prof-delete-file

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

new WasmModuleBuilder().instantiate();
