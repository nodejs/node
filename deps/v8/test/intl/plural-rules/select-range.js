// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const pl = new Intl.PluralRules('sl');
assertEquals('few', pl.selectRange(102, 201));

// Range selection across rounding, compact, and scientific notations.
{
  const en = new Intl.PluralRules('en');
  assertEquals('other', en.selectRange(1, 2));

  const enRound0 = new Intl.PluralRules('en', {
    maximumFractionDigits: 0,
  });
  assertEquals('other', enRound0.selectRange(0.6, 1.6));

  const frRound0 = new Intl.PluralRules('fr', {
    maximumFractionDigits: 0,
  });
  assertEquals('one', frRound0.selectRange(0.1, 0.4));
  assertEquals('one', frRound0.selectRange(0.6, 1.4));
  assertEquals('other', frRound0.selectRange(0.6, 2.4));

  const frCompact = new Intl.PluralRules('fr', {
    notation: 'compact',
    maximumFractionDigits: 0,
  });
  assertEquals('one', frCompact.selectRange(0, 1));
  assertEquals('other', frCompact.selectRange(1, 1_000));

  const frSci = new Intl.PluralRules('fr', {
    notation: 'scientific',
    maximumFractionDigits: 0,
  });
  assertEquals('one', frSci.selectRange(1, 1.4));
  assertEquals('other', frSci.selectRange(1, 2e3));

  const plRules = new Intl.PluralRules('pl');
  assertEquals('few', plRules.selectRange(1, 2));
  assertEquals('many', plRules.selectRange(1, 5));
  assertEquals('other', plRules.selectRange(1, 1.5));

  const plRound0 = new Intl.PluralRules('pl', {
    maximumFractionDigits: 0,
  });
  assertEquals('few', plRound0.selectRange(1.6, 2.4));
  assertEquals('many', plRound0.selectRange(4.6, 5.4));
}
