// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --nostress-incremental-marking

// This test uses a lot of memory and fails with flaky OOM when run
// with --stress-incremental-marking on TSAN.

a = "a".repeat(%StringMaxLength() - 3);
assertThrows(() => new RegExp("a" + a), SyntaxError);
