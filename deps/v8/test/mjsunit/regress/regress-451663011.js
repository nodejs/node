// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const length = 32767;
const pattern_body = "^" + "a".repeat(length);
const pattern = new RegExp("(?<=" + pattern_body + ")", "m");
const input = "a".repeat(length) + "b" + '\n';
try { pattern.exec(input); } catch {}
