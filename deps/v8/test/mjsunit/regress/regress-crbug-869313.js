// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function f() {
  try {
    var a = new ArrayBuffer(1073741824);
    var d = new DataView(a);
    return d.getUint8() === 0;
  } catch(e) {
    return true;
  }
}

!f();
