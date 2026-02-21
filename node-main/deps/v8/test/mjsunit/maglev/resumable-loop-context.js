// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

async function __f_20() {
  try {
    for (let __v_58 = 0; __v_58 < 8; __v_58++) {
    }
  } catch (e) {}
    for (let __v_59 = 1; __v_59 < 1337; __v_59++) {
      function* __f_21() {}
      const __v_66 = __f_21();
      const __v_69 = await "-4294967296";
    }
}

__f_20();
