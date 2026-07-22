// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --noanalyze-environment-liveness --interrupt-budget=1000

for (let __v_1 = 0; __v_1 < 5000; __v_1++) {
  try {
    [].reduce(function () {});
  } catch (__v_2) {}
}

__v_5 = {
  get: function () {
  }
};
