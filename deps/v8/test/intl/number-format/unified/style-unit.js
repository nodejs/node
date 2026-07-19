// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test default.

let nf = new Intl.NumberFormat();
assertEquals(undefined, nf.resolvedOptions().unit);

nf = new Intl.NumberFormat("en");
assertEquals(undefined, nf.resolvedOptions().unit);

nf = new Intl.NumberFormat("en", {style: 'decimal'});
assertEquals(undefined, nf.resolvedOptions().unit);

nf = new Intl.NumberFormat("en", {style: 'currency', currency: 'TWD'});
assertEquals(undefined, nf.resolvedOptions().unit);

nf = new Intl.NumberFormat("en", {style: 'percent'});
assertEquals(undefined, nf.resolvedOptions().unit);

assertThrows(() => new Intl.NumberFormat("en", {style: 'unit'}), TypeError);

const validUnits = [
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
  'microsecond',
  'mile-scandinavian',
  'mile',
  'millimeter',
  'milliliter',
  'millisecond',
  'minute',
  'month',
  'nanosecond',
  'ounce',
  'petabyte',
  'pound',
  'second',
  'stone',
  'terabit',
  'terabyte',
  'week',
  'yard',
  'year',
  'percent',
  'kilometer-per-hour',
  'mile-per-hour',
  'meter-per-second',
  'yard-per-second',
  'yard-per-hour',
  // -per- in IsWellFormedUnitIdentifier
  'liter-per-kilometer',
  'mile-per-gallon',
];

for (const unit of validUnits) {
  let resolved = new Intl.NumberFormat(
      "en", {style: 'unit', unit}).resolvedOptions();
  assertEquals('unit', resolved.style);
  assertEquals(resolved.unit, unit);
}

function c(u) {
  return new Intl.NumberFormat('en', { style: 'unit', unit: u});
}
assertThrows(() => c('acre-foot'), RangeError);
assertThrows(() => c('ampere'), RangeError);
assertThrows(() => c('arc-minute'), RangeError);
assertThrows(() => c('arc-second'), RangeError);
assertThrows(() => c('astronomical-unit'), RangeError);
assertThrows(() => c('bushel'), RangeError);
assertThrows(() => c('calorie'), RangeError);
assertThrows(() => c('carat'), RangeError);
assertThrows(() => c('centiliter'), RangeError);
assertThrows(() => c('century'), RangeError);
assertThrows(() => c('cubic-centimeter'), RangeError);
assertThrows(() => c('cubic-foot'), RangeError);
assertThrows(() => c('cubic-inch'), RangeError);
assertThrows(() => c('cubic-kilometer'), RangeError);
assertThrows(() => c('cubic-meter'), RangeError);
assertThrows(() => c('cubic-mile'), RangeError);
assertThrows(() => c('cubic-yard'), RangeError);
assertThrows(() => c('cup-metric'), RangeError);
assertThrows(() => c('cup'), RangeError);
assertThrows(() => c('day-person'), RangeError);
assertThrows(() => c('deciliter'), RangeError);
assertThrows(() => c('decimeter'), RangeError);
assertThrows(() => c('fathom'), RangeError);
assertThrows(() => c('foodcalorie'), RangeError);
assertThrows(() => c('furlong'), RangeError);
assertThrows(() => c('g-force'), RangeError);
assertThrows(() => c('gallon-imperial'), RangeError);
assertThrows(() => c('generic'), RangeError);
assertThrows(() => c('gigahertz'), RangeError);
assertThrows(() => c('gigawatt'), RangeError);
assertThrows(() => c('hectoliter'), RangeError);
assertThrows(() => c('hectopascal'), RangeError);
assertThrows(() => c('hertz'), RangeError);
assertThrows(() => c('horsepower'), RangeError);
assertThrows(() => c('inch-hg'), RangeError);
assertThrows(() => c('joule'), RangeError);
assertThrows(() => c('karat'), RangeError);
assertThrows(() => c('kelvin'), RangeError);
assertThrows(() => c('kilocalorie'), RangeError);
assertThrows(() => c('kilohertz'), RangeError);
assertThrows(() => c('kilojoule'), RangeError);
assertThrows(() => c('kilowatt-hour'), RangeError);
assertThrows(() => c('kilowatt'), RangeError);
assertThrows(() => c('knot'), RangeError);
assertThrows(() => c('light-year'), RangeError);
assertThrows(() => c('liter-per-100kilometers'), RangeError);
assertThrows(() => c('lux'), RangeError);
assertThrows(() => c('megahertz'), RangeError);
assertThrows(() => c('megaliter'), RangeError);
assertThrows(() => c('megawatt'), RangeError);
assertThrows(() => c('meter-per-second-squared'), RangeError);
assertThrows(() => c('metric-ton'), RangeError);
assertThrows(() => c('microgram'), RangeError);
assertThrows(() => c('micrometer'), RangeError);
assertThrows(() => c('mile-per-gallon-imperial'), RangeError);
assertThrows(() => c('milliampere'), RangeError);
assertThrows(() => c('millibar'), RangeError);
assertThrows(() => c('milligram-per-deciliter'), RangeError);
assertThrows(() => c('milligram'), RangeError);
assertThrows(() => c('millimeter-of-mercury'), RangeError);
assertThrows(() => c('millimole-per-liter'), RangeError);
assertThrows(() => c('milliwatt'), RangeError);
assertThrows(() => c('month-person'), RangeError);
assertThrows(() => c('nanometer'), RangeError);
assertThrows(() => c('nautical-mile'), RangeError);
assertThrows(() => c('ohm'), RangeError);
assertThrows(() => c('ounce-troy'), RangeError);
assertThrows(() => c('parsec'), RangeError);
assertThrows(() => c('part-per-million'), RangeError);
assertThrows(() => c('picometer'), RangeError);
assertThrows(() => c('pint-metric'), RangeError);
assertThrows(() => c('pint'), RangeError);
assertThrows(() => c('pound-per-square-inch'), RangeError);
assertThrows(() => c('quart'), RangeError);
assertThrows(() => c('radian'), RangeError);
assertThrows(() => c('revolution'), RangeError);
assertThrows(() => c('square-centimeter'), RangeError);
assertThrows(() => c('square-foot'), RangeError);
assertThrows(() => c('square-inch'), RangeError);
assertThrows(() => c('square-kilometer'), RangeError);
assertThrows(() => c('square-meter'), RangeError);
assertThrows(() => c('square-mile'), RangeError);
assertThrows(() => c('square-yard'), RangeError);
assertThrows(() => c('tablespoon'), RangeError);
assertThrows(() => c('teaspoon'), RangeError);
assertThrows(() => c('ton'), RangeError);
assertThrows(() => c('volt'), RangeError);
assertThrows(() => c('watt'), RangeError);
assertThrows(() => c('week-person'), RangeError);
assertThrows(() => c('year-person'), RangeError);
