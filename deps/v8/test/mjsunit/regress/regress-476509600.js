// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-struct

const t0 = this.SharedArray;
const v2 = t0();
Object.defineProperties(v2, Object.getOwnPropertyDescriptors(v2));
