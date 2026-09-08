// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-struct

let SType = new SharedStructType(['x']);
let shared = new SType();

let source = {};
for (let i = 0; i < 1021; i++) source['prop' + i] = i;

assertThrows(() => Object.assign(shared, source), TypeError,
             /Cannot add property prop0, object has fixed layout/);
