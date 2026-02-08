// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const v1 = Date.toString();
for (let v2 = 0; v2 < 5; v2++) {
    const v3 = %ConstructThinString(v1);
}
