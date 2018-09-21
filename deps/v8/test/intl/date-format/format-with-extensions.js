// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const d = new Date(2018, 5, 21); // 2018-06-21

function checkFormat(locale, options, expected) {
  let df = new Intl.DateTimeFormat(locale, options);
  let resolvedOptions = df.resolvedOptions();
  assertEquals(expected.cal, resolvedOptions.calendar);
  assertEquals(expected.numSys, resolvedOptions.numberingSystem);

  let formattedParts = df.formatToParts(d);
  let formattedReconstructedFromParts = formattedParts.map((part) => part.value)
      .reduce((accumulated, part) => accumulated + part);
  let formatted = df.format(d);

  assertEquals(formatted, formattedReconstructedFromParts);
  assertEquals(expected.types, formattedParts.map((part) => part.type));
  assertEquals(expected.formatted, formatted);
}

// Even though the calendar is Chinese, the best pattern search for formatting
// should be done in the base locale (i.e. en or en-GB instead of
// en-u-ca-chinese or en-GB-u-ca-chinese). Otherwise, {year: 'numeric'} would
// results in '35 (wu-su)' where 'wu-su' is the designation for year 35 in the
// 60-year cycle. See https://github.com/tc39/ecma402/issues/225 .
[
    ["en", "gregory", "latn", "2018"],
    ["en-GB", "gregory", "latn", "2018"],
    ["en-u-ca-chinese", "chinese", "latn", "35"],
    ["en-GB-u-ca-chinese", "chinese", "latn", "35"],
    ["en-u-ca-chinese-nu-deva", "chinese", "deva", "३५"],
    ["en-GB-u-ca-chinese-nu-deva", "chinese", "deva", "३५"],
].forEach(function(entry) {
  checkFormat(entry[0], {year: 'numeric'},
      { cal: entry[1],
        numSys: entry[2],
        formatted: entry[3],
        types: ["year"],
      });
});

const enUSTypes = ["month", "literal", "day", "literal", "year"];
const enGBTypes = ["day", "literal", "month", "literal", "year"];

[
    ["en", "gregory", "latn", "6/21/2018", enUSTypes],
    ["en-GB", "gregory", "latn", "21/06/2018", enGBTypes],
    ["en-u-nu-deva", "gregory", "deva", "६/२१/२०१८", enUSTypes],
    ["en-u-ca-chinese", "chinese", "latn", "5/8/35", enUSTypes],
    ["en-GB-u-ca-chinese", "chinese", "latn", "08/05/35", enGBTypes],
    ["en-u-ca-chinese-nu-deva", "chinese", "deva", "५/८/३५", enUSTypes],
].forEach(function(entry) {
  checkFormat(entry[0], {},
      { cal: entry[1],
        numSys: entry[2],
        formatted: entry[3],
        types: entry[4],
      });
});
