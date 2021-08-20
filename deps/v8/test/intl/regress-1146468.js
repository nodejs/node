// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test Long Locale won't cause Intl.Locale to assert in debug mode
let lo = new Intl.Locale(
    "de-u-kk-false-ks-level1-kv-space-cu-eur-ms-metric-nu-latn-lb-strict-" +
    "lw-normal-ss-none-em-default-rg-atzzzz-sd-atat1-va-posix");
