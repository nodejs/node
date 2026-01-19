// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags:

const arr = [undefined, -4294967297];
// For the starting index, pass something that is neither undefined nor a Smi
// in order to go to runtime. Here we pass a NaN.
assertTrue(arr.includes(undefined, +arr));
