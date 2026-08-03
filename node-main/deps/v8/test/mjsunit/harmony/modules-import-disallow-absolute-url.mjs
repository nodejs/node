// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var result;
import("https://cdnjs.cloudflare.com/ajax/libs/lodash.js/4.17.21/lodash.min.js").then(
    () => assertUnreachable('Should have failed due to absolute URL'),
    error => result = error.message);

%PerformMicrotaskCheckpoint();

assertMatches('d8: Reading module from https://cdnjs\.cloudflare\.com/.* is not supported.',
    result);
