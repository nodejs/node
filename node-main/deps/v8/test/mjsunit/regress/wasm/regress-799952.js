// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var sentinel = {};
Object.defineProperty(Promise, Symbol.species, {
  value: function(f) {
    f(function() {}, function() {})
    return sentinel;
  }
});

// According to the WebAssembly JavaScript API spec, WebAssembly.instantiate is
// using the initial value of the Promise constructor. Specifically it ignores
// the Promise species constructor installed above.
var promise = WebAssembly.instantiate(new ArrayBuffer());
assertInstanceof(promise, Promise);
assertNotSame(promise, sentinel);

// All further uses of the returned Promise, like using Promise.prototype.then,
// will respect the Promise species constructor installed above however.
var monkey = promise.then(r => { print(r) }, e => { print(e) });
assertSame(monkey, sentinel);
