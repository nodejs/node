// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
assertEquals(
    typeof Intl.Segmenter.supportedLocalesOf,
    "function",
    "Intl.Segmenter.supportedLocalesOf should be a function"
);

const undef = Intl.Segmenter.supportedLocalesOf();
assertEquals([], undef);

const empty = Intl.Segmenter.supportedLocalesOf([]);
assertEquals([], empty);

const strLocale = Intl.Segmenter.supportedLocalesOf("sr");
assertEquals("sr", strLocale[0]);

const multiLocale = ["sr-Thai-RS", "de", "zh-CN"];
assertEquals(multiLocale,
    Intl.Segmenter.supportedLocalesOf(multiLocale, {localeMatcher: "lookup"}));
