// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

["zh", "zh-CN", "zh-Hans", "zh-Hans-CN"].forEach((l) => {
  assertEquals(
      l,
      (new Intl.Collator(
          l, {localeMatcher: "lookup"} )).resolvedOptions().locale);
});
