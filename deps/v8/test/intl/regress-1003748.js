// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let usd = new Intl.NumberFormat('en',
    { style: 'currency', currency: 'USD' }).resolvedOptions();
assertEquals(2, usd.maximumFractionDigits);
assertEquals(2, usd.minimumFractionDigits);

let jpy = new Intl.NumberFormat('en',
    { style: 'currency', currency: 'JPY' }).resolvedOptions();
assertEquals(0, jpy.maximumFractionDigits);
assertEquals(0, jpy.minimumFractionDigits);

let krw = new Intl.NumberFormat('en',
    { style: 'currency', currency: 'KRW' }).resolvedOptions();
assertEquals(0, krw.maximumFractionDigits);
assertEquals(0, krw.minimumFractionDigits);
