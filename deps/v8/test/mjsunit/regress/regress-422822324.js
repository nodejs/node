// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags:

const a = [0.5];
a.push(undefined, undefined);
assertEquals(undefined, a[1]);
assertEquals(undefined, a[2]);
