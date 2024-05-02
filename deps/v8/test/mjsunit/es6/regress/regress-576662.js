// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// https://code.google.com/p/chromium/issues/detail?id=576662 (simplified)

Realm.create();
this.__proto__ = new Proxy({},{});
assertThrows(() => Realm.eval(1, "Realm.global(0).bla = 1"));
