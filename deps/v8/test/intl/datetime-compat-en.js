// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --icu-datetime-compat-lang="en"

let date = new Date("2023-06-14T13:50Z");

assertEquals("9:50:00\u0020AM", date.toLocaleTimeString(
    "en-US", { timeZone: "America/New_York" }));
assertEquals("7:50:00\u202Fa.m.", date.toLocaleTimeString(
    "es-MX", { timeZone: "America/Mexico_City" }));
