// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let midnight = new Date(2019, 3, 4, 0);
let noon = new Date(2019, 3, 4, 12);
let df_11 = new Intl.DateTimeFormat(
    "en", {hour: "numeric", minute: "numeric", hourCycle: "h11"})
assertEquals("h11", df_11.resolvedOptions().hourCycle);
assertEquals("0:00 AM", df_11.formatRange(midnight, midnight));
assertEquals("0:00 PM", df_11.formatRange(noon, noon));

let df_11_t = new Intl.DateTimeFormat(
    "en", {timeStyle: "short", hourCycle: "h11"})
assertEquals("h11", df_11_t.resolvedOptions().hourCycle);
assertEquals("0:00 AM", df_11_t.formatRange(midnight, midnight));
assertEquals("0:00 PM", df_11_t.formatRange(noon, noon));

let df_11_dt = new Intl.DateTimeFormat(
    "en", {timeStyle: "short", dateStyle: "short", hourCycle: "h11"})
assertEquals("h11", df_11_dt.resolvedOptions().hourCycle);
assertEquals("4/4/19, 0:00 AM", df_11_dt.formatRange(midnight, midnight));
assertEquals("4/4/19, 0:00 PM", df_11_dt.formatRange(noon, noon));

let df_12 = new Intl.DateTimeFormat(
    "en", {hour: "numeric", minute: "numeric", hourCycle: "h12"})
assertEquals("h12", df_12.resolvedOptions().hourCycle);
assertEquals("12:00 AM", df_12.formatRange(midnight, midnight));
assertEquals("12:00 PM", df_12.formatRange(noon, noon));

let df_12_t = new Intl.DateTimeFormat(
    "en", {timeStyle: "short", hourCycle: "h12"})
assertEquals("h12", df_12_t.resolvedOptions().hourCycle);
assertEquals("12:00 AM", df_12_t.formatRange(midnight, midnight));
assertEquals("12:00 PM", df_12_t.formatRange(noon, noon));

let df_12_dt = new Intl.DateTimeFormat(
    "en", {timeStyle: "short", dateStyle: "short", hourCycle: "h12"})
assertEquals("h12", df_12_dt.resolvedOptions().hourCycle);
assertEquals("4/4/19, 12:00 AM", df_12_dt.formatRange(midnight, midnight));
assertEquals("4/4/19, 12:00 PM", df_12_dt.formatRange(noon, noon));

let df_23 = new Intl.DateTimeFormat(
    "en", {hour: "numeric", minute: "numeric", hourCycle: "h23"})
assertEquals("h23", df_23.resolvedOptions().hourCycle);
assertEquals("00:00", df_23.formatRange(midnight, midnight));
assertEquals("12:00" ,df_23.formatRange(noon, noon));

let df_23_t = new Intl.DateTimeFormat(
    "en", {timeStyle: "short", hourCycle: "h23"})
assertEquals("h23", df_23_t.resolvedOptions().hourCycle);
assertEquals("00:00", df_23_t.formatRange(midnight, midnight));
assertEquals("12:00" ,df_23_t.formatRange(noon, noon));

let df_23_dt = new Intl.DateTimeFormat(
    "en", {timeStyle: "short", dateStyle: "short", hourCycle: "h23"})
assertEquals("h23", df_23_dt.resolvedOptions().hourCycle);
assertEquals("4/4/19, 00:00", df_23_dt.formatRange(midnight, midnight));
assertEquals("4/4/19, 12:00" ,df_23_dt.formatRange(noon, noon));

let df_24 = new Intl.DateTimeFormat(
    "en", {hour: "numeric", minute: "numeric", hourCycle: "h24"})
assertEquals("h24", df_24.resolvedOptions().hourCycle);
assertEquals("24:00", df_24.formatRange(midnight, midnight));
assertEquals("12:00", df_24.formatRange(noon, noon));

let df_24_t = new Intl.DateTimeFormat(
    "en", {timeStyle: "short", hourCycle: "h24"})
assertEquals("h24", df_24_t.resolvedOptions().hourCycle);
assertEquals("24:00", df_24_t.formatRange(midnight, midnight));
assertEquals("12:00", df_24_t.formatRange(noon, noon));

let df_24_dt = new Intl.DateTimeFormat(
    "en", {timeStyle: "short", dateStyle: "short", hourCycle: "h24"})
assertEquals("h24", df_24_dt.resolvedOptions().hourCycle);
assertEquals("4/4/19, 24:00", df_24_dt.formatRange(midnight, midnight));
assertEquals("4/4/19, 12:00", df_24_dt.formatRange(noon, noon));

let df_11_ja = new Intl.DateTimeFormat(
    "ja-JP", {hour: "numeric", minute: "numeric", hourCycle: "h11"})
assertEquals("h11", df_11_ja.resolvedOptions().hourCycle);
assertEquals("午前0:00", df_11_ja.formatRange(midnight, midnight));
assertEquals("午後0:00", df_11_ja.formatRange(noon, noon));

let df_11_ja_t = new Intl.DateTimeFormat(
    "ja-JP", {timeStyle: "short", hourCycle: "h11"})
assertEquals("h11", df_11_ja_t.resolvedOptions().hourCycle);
assertEquals("午前0:00", df_11_ja_t.formatRange(midnight, midnight));
assertEquals("午後0:00", df_11_ja_t.formatRange(noon, noon));

let df_11_ja_dt = new Intl.DateTimeFormat(
    "ja-JP", {timeStyle: "short", dateStyle: "short", hourCycle: "h11"})
assertEquals("h11", df_11_ja_dt.resolvedOptions().hourCycle);
assertEquals("2019/04/04 午前0:00", df_11_ja_dt.formatRange(midnight, midnight));
assertEquals("2019/04/04 午後0:00", df_11_ja_dt.formatRange(noon, noon));

let df_12_ja = new Intl.DateTimeFormat(
    "ja-JP", {hour: "numeric", minute: "numeric", hourCycle: "h12"})
assertEquals("h12", df_12_ja.resolvedOptions().hourCycle);
assertEquals("午前12:00", df_12_ja.formatRange(midnight, midnight));
assertEquals("午後12:00", df_12_ja.formatRange(noon, noon));

let df_12_ja_t = new Intl.DateTimeFormat(
    "ja-JP", {timeStyle: "short", hourCycle: "h12"})
assertEquals("h12", df_12_ja_t.resolvedOptions().hourCycle);
assertEquals("午前12:00", df_12_ja_t.formatRange(midnight, midnight));
assertEquals("午後12:00", df_12_ja_t.formatRange(noon, noon));

let df_12_ja_dt = new Intl.DateTimeFormat(
    "ja-JP", {timeStyle: "short", dateStyle: "short", hourCycle: "h12"})
assertEquals("h12", df_12_ja_dt.resolvedOptions().hourCycle);
assertEquals("2019/04/04 午前12:00", df_12_ja_dt.formatRange(midnight, midnight));
assertEquals("2019/04/04 午後12:00", df_12_ja_dt.formatRange(noon, noon));

let df_23_ja = new Intl.DateTimeFormat(
    "ja-JP", {hour: "numeric", minute: "numeric", hourCycle: "h23"})
assertEquals("h23", df_23_ja.resolvedOptions().hourCycle);
assertEquals("0:00", df_23_ja.formatRange(midnight, midnight));
assertEquals("12:00", df_23_ja.formatRange(noon, noon));

let df_23_ja_t = new Intl.DateTimeFormat(
    "ja-JP", {timeStyle: "short", hourCycle: "h23"})
assertEquals("h23", df_23_ja_t.resolvedOptions().hourCycle);
assertEquals("0:00", df_23_ja_t.formatRange(midnight, midnight));
assertEquals("12:00", df_23_ja_t.formatRange(noon, noon));

let df_23_ja_dt = new Intl.DateTimeFormat(
    "ja-JP", {timeStyle: "short", dateStyle: "short", hourCycle: "h23"})
assertEquals("h23", df_23_ja_dt.resolvedOptions().hourCycle);
assertEquals("2019/04/04 0:00", df_23_ja_dt.formatRange(midnight, midnight));
assertEquals("2019/04/04 12:00", df_23_ja_dt.formatRange(noon, noon));

let df_24_ja = new Intl.DateTimeFormat(
    "ja-JP", {hour: "numeric", minute: "numeric", hourCycle: "h24"})
assertEquals("h24", df_24_ja.resolvedOptions().hourCycle);
assertEquals("24:00", df_24_ja.formatRange(midnight, midnight));
assertEquals("12:00", df_24_ja.formatRange(noon, noon));

let df_24_ja_t = new Intl.DateTimeFormat(
    "ja-JP", {timeStyle: "short", hourCycle: "h24"})
assertEquals("h24", df_24_ja_t.resolvedOptions().hourCycle);
assertEquals("24:00", df_24_ja_t.formatRange(midnight, midnight));
assertEquals("12:00", df_24_ja_t.formatRange(noon, noon));

let df_24_ja_dt = new Intl.DateTimeFormat(
    "ja-JP", {timeStyle: "short", dateStyle: "short", hourCycle: "h24"})
assertEquals("h24", df_24_ja_dt.resolvedOptions().hourCycle);
assertEquals("2019/04/04 24:00", df_24_ja_dt.formatRange(midnight, midnight));
assertEquals("2019/04/04 12:00", df_24_ja_dt.formatRange(noon, noon));
