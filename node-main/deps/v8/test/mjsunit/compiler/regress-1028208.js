// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-use-ic --interrupt-budget=100 --always-osr

const num_iterations = 1000;
let i = 0;
const re = /foo.bar/;
const RegExpPrototypeExec = RegExp.prototype.exec;
re.exec = function gaga(str) {
    return (i++ < num_iterations) ? RegExpPrototypeExec.call(re, str) : null;
};
re.__defineGetter__("global", () => true);
"foo*bar".match(re);
