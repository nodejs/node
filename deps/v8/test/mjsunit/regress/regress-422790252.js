// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags:

arr = [];
arr.push(undefined, /0/);

assertEquals(undefined, arr[0]);
assertEquals(/0/, arr[1]);
