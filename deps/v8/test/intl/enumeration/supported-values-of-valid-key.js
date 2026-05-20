// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test valid keys
["calendar", "collation", "currency", "numberingSystem", "timeZone", "unit"].forEach(
    function(key) {
        assertDoesNotThrow(() => Intl.supportedValuesOf(key));
        assertEquals("object", typeof Intl.supportedValuesOf(key));
    });
