// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc

var __v_6 = new Boolean();
 __v_6.first = 0;
 __v_6.prop = 1;
for (var __v_2 in __v_6) {
 delete __v_6.prop;
 gc();
}
