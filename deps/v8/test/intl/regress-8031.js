// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-intl-list-format

var locales = ["tlh", "id", "en"];
var input = ["a", "b", "c"];
var referenceListFormat = new Intl.ListFormat(locales);
var referenceFormatted = referenceListFormat.format(input);

class MyFormat extends Intl.ListFormat {
  constructor(locales, options) {
    super(locales, options);
    // could initialize MyListFormat properties
  }
  // could add methods to MyListFormat.prototype
}

var format = new MyFormat(locales);
var actual = format.format(input);
assertEquals(actual, referenceFormatted);
