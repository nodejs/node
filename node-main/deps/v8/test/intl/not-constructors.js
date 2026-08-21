// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

assertThrows(() => new Intl.getCanonicalLocales('en'), TypeError);

let collator = new Intl.Collator('en');
assertThrows(() => new collator.resolvedOptions(), TypeError);

assertThrows(() => new Intl.Collator.supportedLocalesOf('en'), TypeError);

let numberformat = new Intl.NumberFormat('en');
assertThrows(() => new numberformat.resolvedOptions(), TypeError);

assertThrows(() => new Intl.NumberFormat.supportedLocalesOf('en'), TypeError);

let datetimeformat = new Intl.DateTimeFormat('en');
assertThrows(() => new datetimeformat.resolvedOptions(), TypeError);
assertThrows(() => new datetimeformat.formatToParts(new Date()), TypeError);

assertThrows(() => new Intl.DateTimeFormat.supportedLocalesOf('en'), TypeError);

assertThrows(() => new "".localCompare(""), TypeError);

assertThrows(() => new "".normalize(), TypeError);
assertThrows(() => new "".toLocaleLowerCase(), TypeError);
assertThrows(() => new "".toLocaleUpperCase(), TypeError);
assertThrows(() => new "".toLowerCase(), TypeError);
assertThrows(() => new "".toUpperCase(), TypeError);

assertThrows(() => new 3..toLocaleString(), TypeError);
assertThrows(() => new (new Date()).toLocaleString(), TypeError);
assertThrows(() => new (new Date()).toLocaleDateString(), TypeError);
assertThrows(() => new (new Date()).toLocaleTimeString(), TypeError);
