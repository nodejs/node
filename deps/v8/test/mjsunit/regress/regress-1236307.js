// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function __f_2(__v_6) {
  try {
    if (__v_6 > 0) return __f_2(...[__v_6 - 1]);
  } catch (e) {}
}
__f_2(100000);
__f_2(100000);
