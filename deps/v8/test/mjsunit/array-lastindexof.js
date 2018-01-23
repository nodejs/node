// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

assertThrows(() => {
  Array.prototype.lastIndexOf.call(null, 42);
}, TypeError);
assertThrows(() => {
  Array.prototype.lastIndexOf.call(undefined, 42);
}, TypeError);
