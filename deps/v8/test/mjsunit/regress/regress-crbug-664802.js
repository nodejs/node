// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var o = {};
o.__proto__ = new Proxy({}, {});

var m = new Map();
m.set({});
m.set(o);
