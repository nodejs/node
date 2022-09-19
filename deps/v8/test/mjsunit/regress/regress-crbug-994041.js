// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

v0 = Array().join();
RegExp.prototype.__defineSetter__(0, function() {
})
v24 = v0.search();
assertEquals(v24, 0);
