// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

load('sort-base.js');

function SetupPreSorted() {
  CreatePackedSmiArray();
  array_to_sort.sort();
}

function SetupPreSortedReversed() {
  CreatePackedSmiArray();
  array_to_sort.sort();
  array_to_sort.reverse();
}

benchy('PackedSmiPreSorted', Sort, SetupPreSorted);
benchy('PackedSmiPreSortedReversed', Sort, SetupPreSortedReversed);
