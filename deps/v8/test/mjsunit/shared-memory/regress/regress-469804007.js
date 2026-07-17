// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-gc --expose-externalize-string
// Flags: --shared-string-table

for (let i = 0; i < 5; i++) {
    const str = String.fromCharCode(849206214, 0, 0);
    gc();
    externalizeString(str);
    const shared = %ShareObject(str);
    const dummy = "a";
    dummy[shared] = "foo";
}
