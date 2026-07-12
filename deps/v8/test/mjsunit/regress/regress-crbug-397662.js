// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --mock-arraybuffer-allocator --allow-natives-syntax

var a = new Uint8Array(%MaxSmi() >> 1);
a.x = 1;
assertThrows(()=>Object.entries(a), RangeError);
