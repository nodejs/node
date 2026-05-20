// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let calledReturn = false;
Object.prototype.return = function () {
  calledReturn = true;
};
try {
  Object.fromEntries([0]);
} catch(e) {}
assertEquals(true, calledReturn);
