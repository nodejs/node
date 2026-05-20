// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Make sure the "hu" locale format the number group correctly.

let number = 123456.789;
let expected = "123 456,79 Ft";
assertEquals(expected,
    (new Intl.NumberFormat('hu', { style: 'currency', currency: 'HUF'}).format(number)));
assertEquals(expected,
    (new Intl.NumberFormat('hu-HU', { style: 'currency', currency: 'HUF' }).format(number)));
