// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --expose-externalize-string --verify-heap

const LENGTH = 100 * 1000;
const data = new Array();

for (var i = 0; i < LENGTH; ++i) {
  data.push('do ');
}

const largeText = createExternalizableString(data.join());
externalizeString(largeText);
gc();
