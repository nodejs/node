// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --parse-only --stress-runs=2 --throws
// Files: test/mjsunit/regress/non-existent-file-for-558186672.js

// This test verifies that trying to parse a non-existent file with
// --parse-only and --stress-runs does not leak unhandled exceptions onto the
// isolate or crash during Genesis in subsequent runs.
