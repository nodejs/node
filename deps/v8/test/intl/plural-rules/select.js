// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Ordinal plural rules across categories (zero, one, two, few, many, other).
{
  const enOrdinal = new Intl.PluralRules('en', {type: 'ordinal'});
  assertEquals('other', enOrdinal.select(0));
  assertEquals('one', enOrdinal.select(1));
  assertEquals('two', enOrdinal.select(2));
  assertEquals('few', enOrdinal.select(3));
  assertEquals('other', enOrdinal.select(4));
  assertEquals('other', enOrdinal.select(11));
  assertEquals('other', enOrdinal.select(12));
  assertEquals('other', enOrdinal.select(13));
  assertEquals('one', enOrdinal.select(21));
  assertEquals('two', enOrdinal.select(22));
  assertEquals('few', enOrdinal.select(23));
  assertEquals('few', enOrdinal.select(103));

  const cyOrdinal = new Intl.PluralRules('cy', {type: 'ordinal'});
  assertEquals('zero', cyOrdinal.select(0));
  assertEquals('one', cyOrdinal.select(1));
  assertEquals('two', cyOrdinal.select(2));
  assertEquals('few', cyOrdinal.select(3));
  assertEquals('few', cyOrdinal.select(4));
  assertEquals('many', cyOrdinal.select(5));
  assertEquals('many', cyOrdinal.select(6));
  assertEquals('other', cyOrdinal.select(10));
}

// Trailing fraction zeros (operand v) and locale extensions.
{
  const enFixed1 = new Intl.PluralRules('en', {
    minimumFractionDigits: 1,
    maximumFractionDigits: 1,
  });
  assertEquals('other', enFixed1.select(1));

  const arExt = new Intl.PluralRules('ar-u-nu-arab');
  assertEquals('zero', arExt.select(0));
  assertEquals('one', arExt.select(1));
  assertEquals('two', arExt.select(2));
  assertEquals('few', arExt.select(3));
  assertEquals('many', arExt.select(11));
  assertEquals('other', arExt.select(100));
}

// Scientific and engineering notations: negative exponents, > 255 exponents,
// and non-ASCII numbering systems.
{
  const frSci = new Intl.PluralRules('fr', {notation: 'scientific'});
  assertEquals('one', frSci.select(0));
  assertEquals('one', frSci.select(1));
  assertEquals('other', frSci.select(1e5));
  assertEquals('many', frSci.select(1e6));
  assertEquals('many', frSci.select(1.5e6));
  assertEquals('many', frSci.select(1e256));
  assertEquals('many', frSci.select(1e300));

  const enSci = new Intl.PluralRules('en', {notation: 'scientific'});
  assertEquals('one', enSci.select(1));
  assertEquals('other', enSci.select(1e-1));
  assertEquals('other', enSci.select(1e-6));
  assertEquals('other', enSci.select(1e-300));
  assertEquals('other', enSci.select(1e256));
  assertEquals('other', enSci.select(1e300));

  const esSci = new Intl.PluralRules('es', {notation: 'scientific'});
  assertEquals('one', esSci.select(1));
  assertEquals('other', esSci.select(1e5));
  assertEquals('many', esSci.select(1e6));
  assertEquals('many', esSci.select(1e-1));
  assertEquals('many', esSci.select(1e-6));
  assertEquals('many', esSci.select(1e-300));
  assertEquals('many', esSci.select(1e256));
  assertEquals('many', esSci.select(1e300));

  const frEng = new Intl.PluralRules('fr', {notation: 'engineering'});
  assertEquals('one', frEng.select(1));
  assertEquals('other', frEng.select(1e3));
  assertEquals('other', frEng.select(1e5));
  assertEquals('many', frEng.select(1e6));
  assertEquals('many', frEng.select(1e300));

  const frArabSci = new Intl.PluralRules('fr-u-nu-arab', {
    notation: 'scientific',
  });
  assertEquals('many', frArabSci.select(1e6));
  assertEquals('many', frArabSci.select(1e300));
}

// Rounding carries across powers of 10 (including leading digit < 5 with
// roundingIncrement > 1 and ceil/expand rounding modes).
{
  const frCompact = new Intl.PluralRules('fr', {
    notation: 'compact',
    maximumSignificantDigits: 1,
  });
  assertEquals('many', frCompact.select(9.5e5));

  const frEngCeilInc1000 = new Intl.PluralRules('fr', {
    notation: 'engineering',
    minimumFractionDigits: 0,
    maximumFractionDigits: 0,
    roundingIncrement: 1000,
    roundingMode: 'ceil',
  });
  assertEquals('many', frEngCeilInc1000.select(200000));

  const frSciCeilInc10 = new Intl.PluralRules('fr', {
    notation: 'scientific',
    minimumFractionDigits: 0,
    maximumFractionDigits: 0,
    roundingIncrement: 10,
    roundingMode: 'ceil',
  });
  assertEquals('many', frSciCeilInc10.select(150000));

  const frCompactCeilInc1000 = new Intl.PluralRules('fr', {
    notation: 'compact',
    minimumFractionDigits: 0,
    maximumFractionDigits: 0,
    roundingIncrement: 1000,
    roundingMode: 'ceil',
  });
  assertEquals('many', frCompactCeilInc1000.select(200000));
  assertEquals('many', frCompactCeilInc1000.select(150000));

  // Fractional numbers < 1 rounding up to 1 in compact notation.
  const enCompactCeil0 = new Intl.PluralRules('en', {
    notation: 'compact',
    maximumFractionDigits: 0,
    roundingMode: 'ceil',
  });
  assertEquals('one', enCompactCeil0.select(0.1));
  assertEquals('one', enCompactCeil0.select(0.05));
  assertEquals('one', enCompactCeil0.select(0.99));
}
