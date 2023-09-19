// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Regression test about \W w/ i will match S

assertEquals("RST", "RST".replace(/[\W_]/gi, ""));
assertEquals("RST", "RST".replace(/[\W]/gi, ""));
assertEquals("RST", "RST".replace(/[\Wa]/gi, ""));
assertEquals(null, "s".match(/[\u00A0-\u0180]/i));
