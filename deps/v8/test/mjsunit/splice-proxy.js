// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var array = [];
var proxy = new Proxy(new Proxy(array, {}), {});
var Ctor = function() {};
var result;

array.constructor = function() {};
array.constructor[Symbol.species] = Ctor;

Array.prototype.slice.call(proxy);
