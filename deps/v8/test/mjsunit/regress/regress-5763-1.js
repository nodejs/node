// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

try {
  var TA = Object.getPrototypeOf(Int8Array);
  var obj = Reflect.construct(TA, [], Int8Array);
  Int8Array.prototype.values.call(obj).next();
} catch (e) {}
