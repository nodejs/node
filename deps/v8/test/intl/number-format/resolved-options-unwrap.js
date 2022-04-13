// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let nf = Object.create(Intl.NumberFormat.prototype);
nf = Intl.NumberFormat.call(nf);
const actual = Intl.NumberFormat.prototype.resolvedOptions.call(nf);

const expected = new Intl.NumberFormat().resolvedOptions();
Object.keys(expected).forEach(key => assertEquals(expected[key], actual[key]));
assertEquals(Object.keys(expected).length, Object.keys(actual).length);
