// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-intl-segmenter --expose-gc

let segmenter = new Intl.Segmenter();
let segments = segmenter.segment(undefined);
for (let seg of segments) {
  segments = gc();  // free segments and call gc.
}
