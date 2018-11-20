// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

load('sort-base.js');

createSortSuite('PackedSmi', 1000, Sort, CreatePackedSmiArray);
createSortSuite('PackedDouble', 1000, Sort, CreatePackedDoubleArray);
createSortSuite('PackedElement', 1000, Sort, CreatePackedObjectArray);

createSortSuite('HoleySmi', 1000, Sort, CreateHoleySmiArray);
createSortSuite('HoleyDouble', 1000, Sort, CreateHoleyDoubleArray);
createSortSuite('HoleyElement', 1000, Sort, CreateHoleyObjectArray);

createSortSuite('Dictionary', 1000, Sort, CreateDictionaryArray);
