// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('sort-base.js');

// Each benchmark calls sort with multiple different comparison functions
// to create polyomorphic call sites. Most/all of the
// other sort benchmarks have monomorphic call sites.
let sortfn = CreateSortFn([cmp_smaller, cmp_greater]);

createSortSuite('PackedSmi', 1000, sortfn, CreatePackedSmiArray);
createSortSuite('PackedDouble', 1000, sortfn, CreatePackedDoubleArray);
createSortSuite('PackedElement', 1000, sortfn, CreatePackedObjectArray);

createSortSuite('HoleySmi', 1000, sortfn, CreateHoleySmiArray);
createSortSuite('HoleyDouble', 1000, sortfn, CreateHoleyDoubleArray);
createSortSuite('HoleyElement', 1000, sortfn, CreateHoleyObjectArray);

createSortSuite('Dictionary', 1000, sortfn, CreateDictionaryArray);
