// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --js-nonextensible-applies-to-private

d8.debugger.enable();
let p = new Promise(() => {});
Object.preventExtensions(p);
assertDoesNotThrow(() => new Promise(resolve => resolve(p)));
