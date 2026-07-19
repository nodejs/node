// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-icu-default-italian-number-grouping-always

// Default
assertEquals("4321", (4321).toLocaleString("it"));
assertEquals("54.321", (54321).toLocaleString("it"));

// Explicit useGrouping
assertEquals("4.321", (4321).toLocaleString("it", {useGrouping: true}));
assertEquals("54.321", (54321).toLocaleString("it", {useGrouping: true}));
assertEquals("4321", (4321).toLocaleString("it", {useGrouping: false}));
assertEquals("54321", (54321).toLocaleString("it", {useGrouping: false}));
