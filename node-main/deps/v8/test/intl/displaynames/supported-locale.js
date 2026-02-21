// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

assertEquals(typeof Intl.DisplayNames.supportedLocalesOf, "function",
             "Intl.DisplayNames.supportedLocalesOf should be a function");

var undef = Intl.DisplayNames.supportedLocalesOf();
assertEquals([], undef);

var empty = Intl.DisplayNames.supportedLocalesOf([]);
assertEquals([], empty);

var strLocale = Intl.DisplayNames.supportedLocalesOf('sr');
assertEquals('sr', strLocale[0]);

var multiLocale = ['sr-Thai-RS', 'de', 'zh-CN'];
assertEquals(multiLocale,
    Intl.DisplayNames.supportedLocalesOf(multiLocale,
      {localeMatcher: "lookup"}));
