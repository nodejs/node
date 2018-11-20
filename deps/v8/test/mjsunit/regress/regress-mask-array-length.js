// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var a = [];
var o = {
  __proto__: a
};
Object.preventExtensions(o);
o.length = 'abc';
