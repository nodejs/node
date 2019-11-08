// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test precision of compact-rounding

let compact = {notation: "compact"};
assertEquals("1.2K", (1234).toLocaleString("en", compact));
assertEquals("12K", (12345).toLocaleString("en", compact));
assertEquals("123K", (123456).toLocaleString("en", compact));
assertEquals("1.2M", (1234567).toLocaleString("en", compact));

assertEquals("54K", (54321).toLocaleString("en", compact));

let compact_no_rounding = {notation: "compact", minimumFractionDigits: 0};
assertEquals("1.234K", (1234).toLocaleString("en", compact_no_rounding));
assertEquals("12.345K", (12345).toLocaleString("en", compact_no_rounding));
assertEquals("123.456K", (123456).toLocaleString("en", compact_no_rounding));
assertEquals("1.235M", (1234567).toLocaleString("en", compact_no_rounding));

assertEquals("54.321K", (54321).toLocaleString("en", compact_no_rounding));

let fmt = new Intl.NumberFormat("en", compact_no_rounding);
assertEquals("1", fmt.format(1));
assertEquals("1K", fmt.format(1000));
assertEquals("1.234K", fmt.format(1234));
assertEquals("1.25K", fmt.format(1250));
