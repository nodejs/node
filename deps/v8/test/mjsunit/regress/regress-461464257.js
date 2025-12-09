// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --shared-strings

for (let v1 = 0; v1 < 5; v1++) {
    const v3 = %ShareObject(("6").repeat(v1));
    const v4 = %ConstructInternalizedString(v3);
}
