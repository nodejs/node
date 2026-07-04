// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const prefix = "A".repeat(349527);
const suffix = "Ā".repeat(349526);
const code = `"${prefix}${suffix}"`;
eval(code);
