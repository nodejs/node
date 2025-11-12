// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let ta = new Uint8Array([1, 2, 3]);

ta[Symbol.iterator] = 1;
assertThrows(function() { Uint8Array.from(ta); }, TypeError);

ta[Symbol.iterator] = "bad";
assertThrows(function() { Uint8Array.from(ta); }, TypeError);
