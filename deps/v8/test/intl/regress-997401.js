// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test no crash with a very long locale.
let dtf = new Intl.DateTimeFormat(
  'de-u-cu-eur-em-default-hc-h23-ks-level1-lb-strict-lw-normal-ms-metric-nu-latn-rg-atzzzz-sd-atat1-ss-none-tz-atvie-va-posix');
