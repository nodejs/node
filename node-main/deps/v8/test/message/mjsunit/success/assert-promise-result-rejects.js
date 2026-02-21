// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute("test/mjsunit/mjsunit.js");

let obj = {f: 6254};

assertPromiseResult(
    Promise.reject(obj), assertUnreachable, (o) => {print(o.f)});
