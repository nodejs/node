// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --maglev --no-maglev-untagged-phis

function fn1(v2) {
  if (!v2) throw new Error();
}
function fn8() {
  function fn15(v38, v39) {
    let v40 = v38.getInt32(0, v39);
    let v41 = v38.getInt32(0, !v38);
    return [v40, v41];
  }
  let v36 = new ArrayBuffer(8);
  let v37 = new DataView(v36);
  v37.setInt32(0, 0x11223344, true);

  for (let v42 = 0; v42 < 40000; ++v42) { // Make fn8 OSR
    let v43 = fn15(v37, true);
    fn1(v43[0] === 0x11223344);
    fn1(v43[1] === 0x44332211);
  }
}
fn8();
