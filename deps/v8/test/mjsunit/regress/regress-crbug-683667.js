// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --verify-heap

var realm = Realm.create();
var g = Realm.global(realm);
var obj = {x: 0, g: g};

// Navigation will replace JSGlobalObject behind the JSGlobalProxy g and
// therefore will change the g's map. The old map must be marked as non-stable.
Realm.navigate(realm);
gc();
