// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let penny = new Intl.NumberFormat(
    "en", { minimumFractionDigits: 2, maximumFractionDigits: 2, roundingIncrement: 1 });
let nickel = new Intl.NumberFormat(
    "en", { minimumFractionDigits: 2, maximumFractionDigits: 2, roundingIncrement: 5 });
let dime = new Intl.NumberFormat(
    "en", { minimumFractionDigits: 2, maximumFractionDigits: 2, roundingIncrement: 10 });

// https://necs.com/knowledgebase/sysprefs_prc_mod_roundmeth.htm
assertEquals("10.15", penny.format(10.154));
assertEquals("10.16", penny.format(10.155));
assertEquals("10.10", nickel.format(10.124));
assertEquals("10.15", nickel.format(10.125));
assertEquals("10.40", dime.format(10.444));
// mistake in the above page, the result should be 10.40 not 10.50
// assertEquals("10.50", dime.format(10.445));
assertEquals("10.40", dime.format(10.445));
assertEquals("10.50", dime.format(10.45));
