// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test subclassing of Segmenter
class CustomSegmenter extends Intl.Segmenter {
  constructor(locales, options) {
    super(locales, options);
    this.isCustom = true;
  }
}

const seg = new CustomSegmenter("zh");
assertEquals(true, seg.isCustom, "Custom property");
assertEquals(Object.getPrototypeOf(seg), CustomSegmenter.prototype, "Prototype");
