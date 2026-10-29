// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let array = new Uint32Array(32);
array[10] = 10; array[20] = 20;

Array.prototype.sort.call(array);
assertEquals(32, array.length);
assertEquals(10, array[30]);
assertEquals(20, array[31]);
