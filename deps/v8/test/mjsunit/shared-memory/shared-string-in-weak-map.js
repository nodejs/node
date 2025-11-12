// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --shared-string-table --allow-natives-syntax --stress-compaction --expose-gc

const val1 = "some value";
assertTrue(%IsSharedString(val1));

const wm = new WeakMap();
const key1 = {};

wm.set(key1, val1);
assertTrue(wm.get(key1) == val1);
assertTrue(%IsSharedString(wm.get(key1)));

gc();
assertTrue(wm.get(key1) == val1);
assertTrue(%IsSharedString(wm.get(key1)));

%SharedGC();
assertTrue(wm.get(key1) == val1);
assertTrue(%IsSharedString(wm.get(key1)));
