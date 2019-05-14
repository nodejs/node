// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

load('sort-base.js');

function SetupMegamorphic() {
  CreatePackedSmiArray();

  Array.prototype.sort.call([1.1]);
  Array.prototype.sort.call(['a']);
  Array.prototype.sort.call([2,,3]);
  Array.prototype.sort.call([0.2,,0.1]);
  Array.prototype.sort.call(['b',,'a']);
  Array.prototype.sort.call({});
}

createSortSuite('Base', 1000, Sort, SetupMegamorphic);
createSortSuite('MultipleCompareFns', 1000,
    CreateSortFn([cmp_smaller, cmp_greater]), SetupMegamorphic);
