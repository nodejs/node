// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing --expose-gc

const segment = (new Intl.Segmenter).segment();

Sandbox.corruptObjectField(segment, "unicode_string", 0x1);

gc();
new Uint8Array(segment);
