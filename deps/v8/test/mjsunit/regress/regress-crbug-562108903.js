// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Canonicalize for /iu and /iv applies simple case folding to each code point
// separately. Backreference comparison must not use full case folding, under
// which e.g. U+00DF folds to "ss".
//
// Every case is spelled twice: once with literal characters, once with escapes.

// A leading non-Latin1 character forces the two-byte matcher.
const prefix = 'Ā';

function assertBackrefMismatch(pattern, subject) {
  assertFalse(new RegExp(pattern, 'iu').test(subject));
  assertFalse(new RegExp(pattern, 'iv').test(subject));
}

function assertBackrefMatch(pattern, subject) {
  assertTrue(new RegExp(pattern, 'iu').test(subject));
  assertTrue(new RegExp(pattern, 'iv').test(subject));
}

// U+00DF SHARP S has a full folding to "ss" but no simple folding.
assertBackrefMismatch(`^${prefix}(..)\\1$`, `${prefix}ßSsß`);
assertBackrefMismatch(`^${prefix}(..)\\1$`, `${prefix}\u00DFSs\u00DF`);

// U+FB00 LIGATURE FF has a full folding to "ff" but no simple folding.
assertBackrefMismatch(`^${prefix}(..)\\1$`, `${prefix}ﬀFfﬀ`);
assertBackrefMismatch(`^${prefix}(..)\\1$`, `${prefix}\uFB00Ff\uFB00`);

// Simple foldings still match.
assertBackrefMatch(`^${prefix}(..)\\1$`, `${prefix}AbaB`);

assertBackrefMatch(`^${prefix}(.)\\1$`, `${prefix}ßß`);
assertBackrefMatch(`^${prefix}(.)\\1$`, `${prefix}\u00DF\u00DF`);

// Folding across distinct code points needs the case mapping ICU provides.
if (typeof Intl !== 'undefined') {
  // U+1E9E CAPITAL SHARP S folds to U+00DF.
  assertBackrefMatch(`^${prefix}(.)\\1$`, `${prefix}ẞß`);
  assertBackrefMatch(`^${prefix}(.)\\1$`, `${prefix}\u1E9E\u00DF`);

  // U+212A KELVIN SIGN folds to "k".
  assertBackrefMatch(`^${prefix}(.)\\1$`, `${prefix}Kk`);
  assertBackrefMatch(`^${prefix}(.)\\1$`, `${prefix}\u212Ak`);
}
