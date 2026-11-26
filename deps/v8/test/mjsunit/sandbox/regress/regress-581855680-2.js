// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing

globalThis.kUseI64 = true;
d8.file.execute('test/mjsunit/sandbox/regress/regress-581855680-1.js');
