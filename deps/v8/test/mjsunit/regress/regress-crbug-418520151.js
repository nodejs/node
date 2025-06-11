// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --verify-heap --allow-natives-syntax

// Create a new array literal that will provide COW elements.
let arr = [, , , , , , , , true];

// Make sure that the EnsureWritableElements in array.fill triggers a GC.
%SetAllocationTimeout(1, 0, false);
%SimulateNewspaceFull();

// This should succeed without an intermediate heap verification observing
// an array in an invalid state (Smi elements with non-Smi values coped
// from the COW array).
arr.fill(0);
