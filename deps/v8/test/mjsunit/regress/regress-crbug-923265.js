// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let a = {0: 5, 1: 4, 2: 3, length: 2};
Object.freeze(a);

assertThrows(() => Array.prototype.sort.call(a));
assertPropertiesEqual({0: 5, 1: 4, 2: 3, length: 2}, a);
