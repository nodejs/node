// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Fill up the Array prototype's elements.
for (let i = 0; i < 100; i++) Array.prototype.unshift(3.14);

// Create a holey double elements array.
const o31 = [1.1];
o31[37] = 2.2;

// Concat converts to dictionary elements.
const o51 = o31.concat(false);

// Set one element to undefined to trigger the movement bug.
o51[0] = undefined;

assertEquals(o51.length, 39);

// Sort triggers the bug.
o51.sort();

assertEquals(o51.length, 39);
