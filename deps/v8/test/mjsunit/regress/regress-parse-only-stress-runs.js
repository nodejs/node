// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --parse-only --stress-runs=2 --throws

// This test verifies that parsing an invalid file with --parse-only and
// --stress-runs does not leak unhandled exceptions onto the isolate or crash
// during Genesis in subsequent runs.

invalid { javascript code ;
