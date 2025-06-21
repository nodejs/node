// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Non-ascii intrinsic calls shouldn't crash V8.
assertThrows("%ಠ_ಠ()", SyntaxError);
