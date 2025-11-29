// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let defaultFmt = new Intl.NumberFormat("en",
    { minimumFractionDigits: 2, maximumFractionDigits: 2 });
let autoFmt = new Intl.NumberFormat("en",
    { minimumFractionDigits: 2, maximumFractionDigits: 2,
      trailingZeroDisplay: 'auto'});
let stripIfIntegerFmt = new Intl.NumberFormat("en",
    { minimumFractionDigits: 2, maximumFractionDigits: 2,
      trailingZeroDisplay: 'stripIfInteger'});

assertEquals("3.14", defaultFmt.format(3.1411));
assertEquals("3.14", autoFmt.format(3.1411));
assertEquals("3.14", stripIfIntegerFmt.format(3.1411));
assertEquals("3.00", defaultFmt.format(3.001411));
assertEquals("3.00", autoFmt.format(3.001411));
assertEquals("3", stripIfIntegerFmt.format(3.001411));
assertEquals("3.00", defaultFmt.format(2.999411));
assertEquals("3.00", autoFmt.format(2.999411));
assertEquals("3", stripIfIntegerFmt.format(2.999411));
