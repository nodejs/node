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

[
    ["en", "gregory", "latn", "2018"],
    ["en-GB", "gregory", "latn", "2018"],
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
].forEach(function(entry) {
  checkFormat(entry[0], {},
      { cal: entry[1],
        numSys: entry[2],
        formatted: entry[3],
        types: entry[4],
      });
});
