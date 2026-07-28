// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing --expose-externalize-string

let parent = createExternalizableString("A".repeat(120));
externalizeString(parent);
let slice = parent.substring(100);
Sandbox.corruptObjectField(slice, 'offset', 0x80000000);
eval(slice);
