// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

assertEquals(typeof Intl.RelativeTimeFormat.supportedLocalesOf, "function",
             "Intl.RelativeTimeFormat.supportedLocalesOf should be a function");

var undef = Intl.RelativeTimeFormat.supportedLocalesOf();
assertEquals([], undef);

var empty = Intl.RelativeTimeFormat.supportedLocalesOf([]);
assertEquals([], empty);

var strLocale = Intl.RelativeTimeFormat.supportedLocalesOf('sr');
assertEquals('sr', strLocale[0]);

var multiLocale = ['sr-Thai-RS', 'de', 'zh-CN'];
assertEquals(multiLocale,
    Intl.RelativeTimeFormat.supportedLocalesOf(multiLocale,
      {localeMatcher: "lookup"}));
