// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var obj = this;
var handler = {
  has: function() { return false; }
}
var proxy = new Proxy(obj, handler);
Object.defineProperty(obj, "nonconf", {});
assertThrows("'nonconf' in proxy");
