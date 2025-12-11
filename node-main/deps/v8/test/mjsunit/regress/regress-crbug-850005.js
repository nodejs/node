// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let args = [3.34, ];
function f(a, b, c) {};
f(...args);
args = args.splice();
f(...args);
args = [];
f(...args);
