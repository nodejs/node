// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let compileOptions = new Proxy([], {get: () => d8.terminate()});
WebAssembly.compile(new Uint8Array([0]), compileOptions);
