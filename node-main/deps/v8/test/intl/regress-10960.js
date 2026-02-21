// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let d1 = new Date(Date.UTC(2019, 4, 23));

// Ensure calendar: "japanese" under "ja" locale is correct.
assertEquals("R1/5/23", d1.toLocaleDateString(
    "ja", {calendar: "japanese", timeZone:"UTC"}));

assertEquals("令和元年5月23日木曜日", d1.toLocaleDateString(
    "ja", {calendar: "japanese", timeZone:"UTC", dateStyle: "full"}));

assertEquals("令和元年5月23日", d1.toLocaleDateString(
    "ja", {calendar: "japanese", timeZone:"UTC", dateStyle: "long"}));

assertEquals("令和元年5月23日", d1.toLocaleDateString(
    "ja", {calendar: "japanese", timeZone:"UTC", dateStyle: "medium"}));

assertEquals("R1/5/23", d1.toLocaleDateString(
    "ja", {calendar: "japanese", timeZone:"UTC", dateStyle: "short"}));

// Ensure calendar: "chinese" under "zh" locale is correct.
d1 = new Date(Date.UTC(2020, 4, 23));
assertEquals("2020年闰四月1", d1.toLocaleDateString(
    "zh", {calendar: "chinese", timeZone:"UTC"}));

assertEquals("2020庚子年闰四月初一星期六", d1.toLocaleDateString(
    "zh", {calendar: "chinese", timeZone:"UTC", dateStyle: "full"}));

assertEquals("2020庚子年闰四月初一", d1.toLocaleDateString(
    "zh", {calendar: "chinese", timeZone:"UTC", dateStyle: "long"}));

assertEquals("2020年闰四月初一", d1.toLocaleDateString(
    "zh", {calendar: "chinese", timeZone:"UTC", dateStyle: "medium"}));

assertEquals("2020/闰4/1", d1.toLocaleDateString(
    "zh", {calendar: "chinese", timeZone:"UTC", dateStyle: "short"}));
