// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-proxies

var fuse = 1;
var handler = {
  get: function() { return function() {} },
  getPropertyDescriptor: function() {
    if (fuse-- == 0) throw "please die";
    return {value: function() {}, configurable: true};
  }
};

var p = Proxy.create(handler);
var o = Object.create(p);
with (o) { f() }
