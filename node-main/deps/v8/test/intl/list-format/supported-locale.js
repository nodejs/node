// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

assertEquals(typeof Intl.ListFormat.supportedLocalesOf, "function",
             "Intl.ListFormat.supportedLocalesOf should be a function");

var undef = Intl.ListFormat.supportedLocalesOf();
assertEquals([], undef);

var empty = Intl.ListFormat.supportedLocalesOf([]);
assertEquals([], empty);

var strLocale = Intl.ListFormat.supportedLocalesOf('sr');
assertEquals('sr', strLocale[0]);

var multiLocale = ['sr-Thai-RS', 'de', 'zh-CN'];
assertEquals(multiLocale,
    Intl.ListFormat.supportedLocalesOf(multiLocale, {localeMatcher: "lookup"}));
