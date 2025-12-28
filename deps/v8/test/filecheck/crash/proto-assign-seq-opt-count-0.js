// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --proto-assign-seq-opt --proto-assign-seq-opt-count=0

// --proto-assign-seq-opt-count=0 disables --proto-assign-seq-opt. In that case
// the actual flag value is never used. If the feature flag itself is enabled,
// this causes a contradiction crashing the program.
// CHECK: Flag processing error
