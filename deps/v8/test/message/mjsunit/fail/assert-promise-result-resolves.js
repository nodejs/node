// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

load("test/mjsunit/mjsunit.js");

let obj = {f: 1254};

assertPromiseResult(
    Promise.resolve(obj), _ => {throw new Error('Error in resolve handler')},
    (o) => {print(o.f)});
