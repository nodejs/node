// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-intl-segmenter
assertEquals(
    typeof Intl.Segmenter.supportedLocalesOf,
    "function",
    "Intl.Segmenter.supportedLocalesOf should be a function"
);

var undef = Intl.Segmenter.supportedLocalesOf();
assertEquals([], undef);

var empty = Intl.Segmenter.supportedLocalesOf([]);
assertEquals([], empty);

var strLocale = Intl.Segmenter.supportedLocalesOf("sr");
assertEquals("sr", strLocale[0]);

var multiLocale = ["sr-Thai-RS", "de", "zh-CN"];
assertEquals(multiLocale, Intl.Segmenter.supportedLocalesOf(multiLocale));
