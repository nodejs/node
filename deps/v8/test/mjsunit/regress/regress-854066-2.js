// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

oobArray = [];
delete oobArray.__proto__[Symbol.iterator];
for (let i = 0; i < 1e5; ++i) {
  oobArray[i] = 1.1;
}
floatArray = new Float64Array(oobArray.length);
Float64Array.from.call(function(length) {
  oobArray.length = 0;
  return floatArray;
}, oobArray);
