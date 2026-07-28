// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --fuzzing

function f0() {
    return f0;
}
const v2 = %ConstructInternalizedString(`Av9O${f0}symbol`);
