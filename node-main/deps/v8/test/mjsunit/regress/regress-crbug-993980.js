// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function () {
    // generate some sample data
    let data = new Array(1600).fill(null).map((e, i) => ({
        invariantKey: 'v',
        ['randomKey' + i]: 'w',

    }));

    // use json parser
    data = JSON.parse(JSON.stringify(data))

    // look for undefined values
    for (const t of data) {
      assertFalse(Object.keys(t).some(k => !t[k]));
    }
})()
