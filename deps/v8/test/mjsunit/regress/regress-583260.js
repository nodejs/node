// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

__v_1 = {
  has() { return true }
};
__v_2 = new Proxy({}, __v_1);
function __f_5(object) {
  with (object) { return delete __v_3; }
}
 __f_5(__v_2)
