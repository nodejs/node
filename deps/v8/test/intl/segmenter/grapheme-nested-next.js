// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test to ensure the nested calling of the next method won't caused
// confusion to each other.
const segmenter = new Intl.Segmenter();
const input = "ABCD";
const segments = segmenter.segment(input);
let result = [];
for (let v1 of segments) {
  for (let v2 of segments) {
    result.push(v1.segment);
    result.push(v2.segment);
  }
  result.push(":");
}
assertEquals("AAABACAD:BABBBCBD:CACBCCCD:DADBDCDD:", result.join(""));
