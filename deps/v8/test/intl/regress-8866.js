// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Regression test to ensure Intl.PluralRules respect minimumFractionDigits

assertEquals('other',
    new Intl.PluralRules("en", {minimumFractionDigits: 2}).select(1));

assertEquals('zero',
    new Intl.PluralRules("lv", {minimumFractionDigits: 2}).select(1.13));
