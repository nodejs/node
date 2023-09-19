// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var constructorArgs = new Array(0x10100);
var constructor = function() {};
var target = new Proxy(constructor, {
  construct: function() {
  }
});
var proxy = new Proxy(target, {
  construct: function(newTarget, args) {
    return Reflect.construct(constructor, []);
  }
});
var instance = new proxy();
var instance2 = Reflect.construct(proxy, constructorArgs);
%HeapObjectVerify(target);
%HeapObjectVerify(proxy);
%HeapObjectVerify(instance);
%HeapObjectVerify(instance2);
