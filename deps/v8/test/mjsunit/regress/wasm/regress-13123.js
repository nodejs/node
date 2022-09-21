// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Object.defineProperty(this, "WebAssembly", { get: () => { throw "nope" } });
assertThrowsEquals(() => d8.test.installConditionalFeatures(), 'nope');
