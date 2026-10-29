// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Environment Variables: LC_ALL=de

assertEquals("de", (new Intl.Collator([])).resolvedOptions().locale);
assertEquals("de", (new Intl.Collator(['xx'])).resolvedOptions().locale);
assertEquals("de", (new Intl.Collator(undefined)).resolvedOptions().locale);
assertEquals("de", (new Intl.Collator(undefined, {usage: 'sort'})).resolvedOptions().locale);
assertEquals("de", (new Intl.Collator(undefined, {usage: 'search'})).resolvedOptions().locale);
assertEquals("de", (new Intl.DateTimeFormat([])).resolvedOptions().locale);
assertEquals("de", (new Intl.DateTimeFormat(['xx'])).resolvedOptions().locale);
assertEquals("de", (new Intl.NumberFormat([])).resolvedOptions().locale);
assertEquals("de", (new Intl.NumberFormat(['xx'])).resolvedOptions().locale);
assertEquals("de", (new Intl.v8BreakIterator([])).resolvedOptions().locale);
assertEquals("de", (new Intl.v8BreakIterator(['xx'])).resolvedOptions().locale);
