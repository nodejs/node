// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let arr = new Array(10);
Object.defineProperty(arr, 0, {value: 10, writable: false});
Object.defineProperty(arr, 9, {value: 1, writable: false});

assertThrows(() => arr.sort(), TypeError);
