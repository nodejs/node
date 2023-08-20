// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
let t = new Uint8Array([1, 2, 3, 4, 5, 6, 7, 8]);
let t2 = new DataView(t.buffer, 7, {valueOf() { return -1; }});
