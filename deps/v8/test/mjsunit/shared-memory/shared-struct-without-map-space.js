// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --no-use-map-space --harmony-struct

"use strict";

// Test ensures that deserialization works without map space and
// that we can allocate maps in the shared heap.

let SomeStruct = new SharedStructType(['field1', 'field2']);
