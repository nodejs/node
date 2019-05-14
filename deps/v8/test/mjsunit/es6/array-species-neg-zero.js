// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * 9.4.2.3 ArraySpeciesCreate(originalArray, length)
 *
 * 1. Assert: length is an integer Number ≥ 0.
 * 2. If length is −0, let length be +0.
 * [...]
 */

var x = [];
var deleteCount;

x.constructor = function() {};
x.constructor[Symbol.species] = function(param) {
  deleteCount = param;
};

x.splice(0, -0);

assertEquals(0, deleteCount);
