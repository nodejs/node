// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute("test/mjsunit/mjsunit.js");

let obj = {f: 1254};

assertPromiseResult(
    Promise.reject(obj), (o) => {print(o.f)},
    _ => {throw new Error('Error in reject handler')});
