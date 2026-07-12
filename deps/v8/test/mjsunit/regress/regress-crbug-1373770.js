// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

JSON.parse('[0,0]', function (a, b, c) {
    console.log(a);
    console.log(b);
    console.log(c);
    quit();
});
