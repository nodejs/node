// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const s = 'data:text/javascript,' + '\u0080'.repeat(1 << 19);

assertThrows(() => load(s), Error);
assertThrows(() => new Worker(s, {type: 'string'}), Error);
assertPromiseResult(import(s), assertUnreachable, () => {});
