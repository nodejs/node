// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let useArgs = undefined;
function f(arg) {
    useArgs = 'result' + arguments[0] + arg;
}

Intl.NumberFormat.__proto__ = { [Symbol.hasInstance]: f };

new Intl.NumberFormat();
