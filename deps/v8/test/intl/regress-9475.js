// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test format of all valid units won't throw exception.

let validList = [
  // IsSanctionedSimpleUnitIdentifier
  'acre',
  'bit',
  'byte',
  'celsius',
  'centimeter',
  'day',
  'degree',
  'fahrenheit',
  'fluid-ounce',
  'foot',
  'gallon',
  'gigabit',
  'gigabyte',
  'gram',
  'hectare',
  'hour',
  'inch',
  'kilobit',
  'kilobyte',
  'kilogram',
  'kilometer',
  'liter',
  'megabit',
  'megabyte',
  'meter',
  'mile',
  'mile-scandinavian',
  'millimeter',
  'milliliter',
  'millisecond',
  'minute',
  'month',
  'ounce',
  'percent',
  'petabyte',
  'pound',
  'second',
  'stone',
  'terabit',
  'terabyte',
  'week',
  'yard',
  'year',
  // -per- in IsWellFormedUnitIdentifier
  'liter-per-kilometer',
  'mile-per-gallon',
];

for (let unit of validList) {
  let nf = new Intl.NumberFormat("en", {style: "unit", unit});
  assertDoesNotThrow(() => nf.format(123.45),
      "unit: '" + unit + "' should not throw");
}
