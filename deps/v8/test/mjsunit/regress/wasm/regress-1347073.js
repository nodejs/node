// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var values = [];
values.length = 1;
Object.prototype.__defineGetter__(0, function() {
  throw new Error('foo');
});
let tag = new WebAssembly.Tag({parameters: ['externref']});
assertThrows(() => new WebAssembly.Exception(tag, values), Error, 'foo');
