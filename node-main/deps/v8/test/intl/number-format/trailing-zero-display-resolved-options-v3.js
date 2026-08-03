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

assertEquals("auto", defaultFmt.resolvedOptions().trailingZeroDisplay);
assertEquals("auto", autoFmt.resolvedOptions().trailingZeroDisplay);
assertEquals("stripIfInteger",
    stripIfIntegerFmt.resolvedOptions().trailingZeroDisplay);
