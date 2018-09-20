// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function opt() {
    let opt, arr = [...[...[...[...new Uint8Array(0x10000)]]]];
    while (arr--)  {
        opt = ((typeof opt) === 'undefined') ? /a/ : arr;
    }
}
opt();
opt();
