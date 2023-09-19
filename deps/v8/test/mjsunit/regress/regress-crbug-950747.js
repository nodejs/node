// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let o = {};
Reflect.set(o, "a", 0.1);

let o1 = {};
o1.a = {};

Reflect.set(o, "a", 0.1);
