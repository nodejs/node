// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let midnight = new Date(2019, 3, 4, 0);
let noon = new Date(2019, 3, 4, 12);
let df_11 = new Intl.DateTimeFormat(
    "en", {hour: "numeric", minute: "numeric", hourCycle: "h11"})
assertEquals("h11", df_11.resolvedOptions().hourCycle);
assertEquals("0:00 AM", df_11.format(midnight));
assertEquals("0:00 PM", df_11.format(noon));

let df_12 = new Intl.DateTimeFormat(
    "en", {hour: "numeric", minute: "numeric", hourCycle: "h12"})
assertEquals("h12", df_12.resolvedOptions().hourCycle);
assertEquals("12:00 AM", df_12.format(midnight));
assertEquals("12:00 PM", df_12.format(noon));

let df_23 = new Intl.DateTimeFormat(
    "en", {hour: "numeric", minute: "numeric", hourCycle: "h23"})
assertEquals("h23", df_23.resolvedOptions().hourCycle);
assertEquals("00:00", df_23.format(midnight));
assertEquals("12:00" ,df_23.format(noon));

let df_24 = new Intl.DateTimeFormat(
    "en", {hour: "numeric", minute: "numeric", hourCycle: "h24"})
assertEquals("h24", df_24.resolvedOptions().hourCycle);
assertEquals("24:00", df_24.format(midnight));
assertEquals("12:00", df_24.format(noon));

let df_11_ja = new Intl.DateTimeFormat(
    "ja-JP", {hour: "numeric", minute: "numeric", hourCycle: "h11"})
assertEquals("h11", df_11_ja.resolvedOptions().hourCycle);
assertEquals("午前0:00", df_11_ja.format(midnight));
assertEquals("午後0:00", df_11_ja.format(noon));

let df_12_ja = new Intl.DateTimeFormat(
    "ja-JP", {hour: "numeric", minute: "numeric", hourCycle: "h12"})
assertEquals("h12", df_12_ja.resolvedOptions().hourCycle);
assertEquals("午前12:00", df_12_ja.format(midnight));
assertEquals("午後12:00", df_12_ja.format(noon));

let df_23_ja = new Intl.DateTimeFormat(
    "ja-JP", {hour: "numeric", minute: "numeric", hourCycle: "h23"})
assertEquals("h23", df_23_ja.resolvedOptions().hourCycle);
assertEquals("0:00", df_23_ja.format(midnight));
assertEquals("12:00", df_23_ja.format(noon));

let df_24_ja = new Intl.DateTimeFormat(
    "ja-JP", {hour: "numeric", minute: "numeric", hourCycle: "h24"})
assertEquals("h24", df_24_ja.resolvedOptions().hourCycle);
assertEquals("24:00", df_24_ja.format(midnight));
assertEquals("12:00", df_24_ja.format(noon));
