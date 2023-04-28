// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test that fast COW arrays are properly handled by Array#sort.

function COWSort() {
  const array = ["cc", "c", "aa", "bb", "b", "ab", "ac"];
  array.sort();
  return array;
}

assertArrayEquals(["aa", "ab", "ac", "b", "bb", "c", "cc"], COWSort());

Array.prototype.sort = () => {};

assertArrayEquals(["cc", "c", "aa", "bb", "b", "ab", "ac"], COWSort());
