// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --stress-lazy --stress-lazy-source-positions

function* __f_2(__v_80) {
  try {
      () => WebAssembly.compileStreaming( __v_80);
      function __f_6() {
        try {
            let __v_91 = 254;
            function __f_8(__v_92) {
              try {
                [...[], __v_92 / 8, ...[7, __v_91]];
              } catch (e) {
                print(__v_91)
              }
            }
        } catch (e) {}
      }
  } catch (e) {}
}
