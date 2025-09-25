// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var result;
// Hopefully a larger number of ".." than the current directory level.
import("../".repeat(42)).then(
    () => assertUnreachable('Should have failed due to non-existing module'),
    error => result = error.message);

%PerformMicrotaskCheckpoint();

assertMatches('d8: Error reading module from ', result);
