// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-struct --allow-natives-syntax

// Creating a SharedStructType with an "interesting" property like "toJSON"
// should not fail verification.
const StructType = this.SharedStructType(["toJSON"]);
const instance = StructType();
%HeapObjectVerify(instance);
