// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc

var obj = {}
obj.__proto__ = null;
Object.defineProperty(obj, "prop", {
  set: gc
});
for (var i = 0; i < 100 ; ++i) {
  obj["prop"] = 0;
}
