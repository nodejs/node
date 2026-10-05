// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-struct

let MyStruct = new SharedStructType(['field']);
let obj = new MyStruct();
assertThrows(() => Function[Symbol.hasInstance].call(MyStruct, obj),
             TypeError, /Function has non-object prototype 'null'/);
