// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

for (x = 0; x < 10000; ++x) {
    [(x) => x, [, 4294967295].find((x) => x), , 2].includes('x', -0);
}
