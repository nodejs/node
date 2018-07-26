// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

load('sort-base.js');

// Each benchmark calls sort with multiple different comparison functions
// to create polyomorphic call sites. Most/all of the
// other sort benchmarks have monomorphic call sites.

let sortfn = CreateSortFn([cmp_smaller, cmp_greater]);

benchy('PackedSmi', sortfn, CreatePackedSmiArray);
benchy('PackedDouble', sortfn, CreatePackedDoubleArray);
benchy('PackedElement', sortfn, CreatePackedObjectArray);

benchy('HoleySmi', sortfn, CreateHoleySmiArray);
benchy('HoleyDouble', sortfn, CreateHoleyDoubleArray);
benchy('HoleyElement', sortfn, CreateHoleyObjectArray);

benchy('Dictionary', sortfn, CreateDictionaryArray);
