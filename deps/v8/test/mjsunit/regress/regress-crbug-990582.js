// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --invoke-weak-callbacks --budget-for-feedback-vector-allocation=0

__v_0 = 0;
function __f_0() {
   try {
     __f_0();
   } catch(e) {
     if (__v_0 < 50) {
       __v_0++;
/()/g, new [];
     }
   }
}
  __f_0();
Realm.shared = this;
