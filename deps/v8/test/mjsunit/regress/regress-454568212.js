// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const re = new RegExp("(a)\\1" + "b".repeat(32767));
try { "".match(re); } catch {}
