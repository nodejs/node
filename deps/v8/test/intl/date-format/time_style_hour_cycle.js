// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let midnight = new Date(2019, 3, 4, 0);
let noon = new Date(2019, 3, 4, 12);

let df_11_t = new Intl.DateTimeFormat(
    "en", {timeStyle: "short", hourCycle: "h11"})
assertEquals("h11", df_11_t.resolvedOptions().hourCycle);
assertEquals("0:00 AM", df_11_t.format(midnight));
assertEquals("0:00 PM", df_11_t.format(noon));

let df_12_t = new Intl.DateTimeFormat(
    "en", {timeStyle: "short", hourCycle: "h12"})
assertEquals("h12", df_12_t.resolvedOptions().hourCycle);
assertEquals("12:00 AM", df_12_t.format(midnight));
assertEquals("12:00 PM", df_12_t.format(noon));

let df_23_t = new Intl.DateTimeFormat(
    "en", {timeStyle: "short", hourCycle: "h23"})
assertEquals("h23", df_23_t.resolvedOptions().hourCycle);
assertEquals("00:00", df_23_t.format(midnight));
assertEquals("12:00" ,df_23_t.format(noon));

let df_24_t = new Intl.DateTimeFormat(
    "en", {timeStyle: "short", hourCycle: "h24"})
assertEquals("h24", df_24_t.resolvedOptions().hourCycle);
assertEquals("24:00", df_24_t.format(midnight));
assertEquals("12:00", df_24_t.format(noon));

let df_11_ja_t = new Intl.DateTimeFormat(
    "ja-JP", {timeStyle: "short", hourCycle: "h11"})
assertEquals("h11", df_11_ja_t.resolvedOptions().hourCycle);
assertEquals("午前0:00", df_11_ja_t.format(midnight));
assertEquals("午後0:00", df_11_ja_t.format(noon));

let df_12_ja_t = new Intl.DateTimeFormat(
    "ja-JP", {timeStyle: "short", hourCycle: "h12"})
assertEquals("h12", df_12_ja_t.resolvedOptions().hourCycle);
assertEquals("午前12:00", df_12_ja_t.format(midnight));
assertEquals("午後12:00", df_12_ja_t.format(noon));

let df_23_ja_t = new Intl.DateTimeFormat(
    "ja-JP", {timeStyle: "short", hourCycle: "h23"})
assertEquals("h23", df_23_ja_t.resolvedOptions().hourCycle);
assertEquals("0:00", df_23_ja_t.format(midnight));
assertEquals("12:00", df_23_ja_t.format(noon));

let df_24_ja_t = new Intl.DateTimeFormat(
    "ja-JP", {timeStyle: "short", hourCycle: "h24"})
assertEquals("h24", df_24_ja_t.resolvedOptions().hourCycle);
assertEquals("24:00", df_24_ja_t.format(midnight));
assertEquals("12:00", df_24_ja_t.format(noon));
