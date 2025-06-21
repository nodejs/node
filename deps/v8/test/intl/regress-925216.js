// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

assertTrue(new Intl.DateTimeFormat(
    "en", { timeZone: 'UTC', hour: 'numeric'}).resolvedOptions().hour12);
assertFalse(new Intl.DateTimeFormat(
    "fr", { timeZone: 'UTC', hour: 'numeric'}).resolvedOptions().hour12);
assertFalse(new Intl.DateTimeFormat(
    "de", { timeZone: 'UTC', hour: 'numeric'}).resolvedOptions().hour12);
