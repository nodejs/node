// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function HasPackedSmiElements(xs) {
  return %HasFastPackedElements(xs) && %HasSmiElements(xs);
}

function HasPackedDoubleElements(xs) {
  return %HasFastPackedElements(xs) && %HasDoubleElements(xs);
}

function HasPackedObjectElements(xs) {
  return %HasFastPackedElements(xs) && %HasObjectElements(xs);
}

function HasHoleySmiElements(xs) {
  return %HasHoleyElements(xs) && %HasSmiElements(xs);
}

function HasHoleyDoubleElements(xs) {
  return %HasHoleyElements(xs) && %HasDoubleElements(xs);
}

function HasHoleyObjectElements(xs) {
  return %HasHoleyElements(xs) && %HasObjectElements(xs);
}

function MakeArrayDictionaryMode(array, elementCreationCallback) {
  // Add elements into the array until it turns dictionary mode.
  let i = 0;
  while (!%HasDictionaryElements(array)) {
    array[i] = elementCreationCallback();
    i += 100;
  }
}
