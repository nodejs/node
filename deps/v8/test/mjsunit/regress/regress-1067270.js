// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

const needle = Array(1802).join(" +") + Array(16884).join("A");
const string = "A";

assertEquals(string.search(needle), -1);
assertEquals(string.search(needle), -1);
