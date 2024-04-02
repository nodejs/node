// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let midnight = new Date(2019, 3, 4, 0);
let noon = new Date(2019, 3, 4, 12);

let df_11_dt = new Intl.DateTimeFormat(
    "en", {timeStyle: "short", dateStyle: "short", hourCycle: "h11"})
assertEquals("h11", df_11_dt.resolvedOptions().hourCycle);
assertEquals("4/4/19, 0:00 AM", df_11_dt.format(midnight));
assertEquals("4/4/19, 0:00 PM", df_11_dt.format(noon));

let df_12_dt = new Intl.DateTimeFormat(
    "en", {timeStyle: "short", dateStyle: "short", hourCycle: "h12"})
assertEquals("h12", df_12_dt.resolvedOptions().hourCycle);
assertEquals("4/4/19, 12:00 AM", df_12_dt.format(midnight));
assertEquals("4/4/19, 12:00 PM", df_12_dt.format(noon));

let df_23_dt = new Intl.DateTimeFormat(
    "en", {timeStyle: "short", dateStyle: "short", hourCycle: "h23"})
assertEquals("h23", df_23_dt.resolvedOptions().hourCycle);
assertEquals("4/4/19, 00:00", df_23_dt.format(midnight));
assertEquals("4/4/19, 12:00" ,df_23_dt.format(noon));

let df_24_dt = new Intl.DateTimeFormat(
    "en", {timeStyle: "short", dateStyle: "short", hourCycle: "h24"})
assertEquals("h24", df_24_dt.resolvedOptions().hourCycle);
assertEquals("4/4/19, 24:00", df_24_dt.format(midnight));
assertEquals("4/4/19, 12:00", df_24_dt.format(noon));

let df_11_ja_dt = new Intl.DateTimeFormat(
    "ja-JP", {timeStyle: "short", dateStyle: "short", hourCycle: "h11"})
assertEquals("h11", df_11_ja_dt.resolvedOptions().hourCycle);
assertEquals("2019/04/04 午前0:00", df_11_ja_dt.format(midnight));
assertEquals("2019/04/04 午後0:00", df_11_ja_dt.format(noon));

let df_12_ja_dt = new Intl.DateTimeFormat(
    "ja-JP", {timeStyle: "short", dateStyle: "short", hourCycle: "h12"})
assertEquals("h12", df_12_ja_dt.resolvedOptions().hourCycle);
assertEquals("2019/04/04 午前12:00", df_12_ja_dt.format(midnight));
assertEquals("2019/04/04 午後12:00", df_12_ja_dt.format(noon));

let df_23_ja_dt = new Intl.DateTimeFormat(
    "ja-JP", {timeStyle: "short", dateStyle: "short", hourCycle: "h23"})
assertEquals("h23", df_23_ja_dt.resolvedOptions().hourCycle);
assertEquals("2019/04/04 0:00", df_23_ja_dt.format(midnight));
assertEquals("2019/04/04 12:00", df_23_ja_dt.format(noon));

let df_24_ja_dt = new Intl.DateTimeFormat(
    "ja-JP", {timeStyle: "short", dateStyle: "short", hourCycle: "h24"})
assertEquals("h24", df_24_ja_dt.resolvedOptions().hourCycle);
assertEquals("2019/04/04 24:00", df_24_ja_dt.format(midnight));
assertEquals("2019/04/04 12:00", df_24_ja_dt.format(noon));
