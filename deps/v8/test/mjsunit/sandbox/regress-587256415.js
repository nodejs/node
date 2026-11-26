// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing

const re = new RegExp(
    '^(?=((?=b|)(?:a(?=c)|)))(.)(?<=\\2' + '\\1'.repeat(50) + ')', 'iu');
const subject = 'aĀ';

// Compile the two-byte code. Capture 1 is empty.
assertEquals(['a', '', 'a'], re.exec(subject));

// With this length, positions in the subject no longer fit in 32 bits and
// capture 1 is recorded as 4GB long. Each \1 in the lookbehind moves the
// position back by that much. Together they must not step over the guard
// region in front of the sandbox, where \2 would then read.
Sandbox.corruptObjectField(subject, 'length', 0x80000001);
re.exec(subject);
