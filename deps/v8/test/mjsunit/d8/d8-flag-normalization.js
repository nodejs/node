// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: -disallow_unsafe_flags -flag_processing_mode=ignore-contradictions
// Flags: --enable-os-system --noarguments

assertEquals(undefined, os.system);
assertEquals('undefined', typeof arguments);
