// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let __v_0 = [];
Object.defineProperty(__v_0, 'length', { writable: false });
function __f_5() { return  __v_0;  }
assertThrows(() => Array.of.call(__f_5), TypeError);
