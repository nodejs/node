// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

load('sort-base.js');

benchy('PackedSmi', Sort, CreatePackedSmiArray);
benchy('PackedDouble', Sort, CreatePackedDoubleArray);
benchy('PackedElement', Sort, CreatePackedObjectArray);

benchy('HoleySmi', Sort, CreateHoleySmiArray);
benchy('HoleyDouble', Sort, CreateHoleyDoubleArray);
benchy('HoleyElement', Sort, CreateHoleyObjectArray);

benchy('Dictionary', Sort, CreateDictionaryArray);
