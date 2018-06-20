// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class MyPromise extends Promise {
    static get [Symbol.species]() {
        return function(f) {
            console.log("foo")
            var a = new Promise(f);
            return new Proxy(new Function(),{})
        }
    }
}
var p1 = new Promise(function(resolve, reject) {});
p1.__proto__ = MyPromise.prototype;
p1.then();
p1.then();

for (var i = 0; i < 0x20000; i++) {
    new String()
}
