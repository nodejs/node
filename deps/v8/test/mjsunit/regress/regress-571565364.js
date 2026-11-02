// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Resources: test/mjsunit/regress/regress-571565364-empty.txt

const buffer = readbuffer('test/mjsunit/regress/regress-571565364-empty.txt');
assertEquals(0, buffer.byteLength);
