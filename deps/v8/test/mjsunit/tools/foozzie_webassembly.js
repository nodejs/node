// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax
// Files: tools/clusterfuzz/v8_mock.js
// Files: tools/clusterfuzz/v8_mock_webassembly.js

// Test foozzie webassembly-specfific mocks for differential fuzzing.

// No reference errors when accessing WebAssembly.
WebAssembly[0];
WebAssembly[" "];
WebAssembly.foo;
WebAssembly.foo();
WebAssembly.foo().bar;
WebAssembly.foo().bar();
WebAssembly.foo().bar[0];
