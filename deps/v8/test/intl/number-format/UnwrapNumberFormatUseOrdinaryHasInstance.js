// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Verify ECMA402 PR 500 Use OrdinaryHasInstance in normative optional steps
// https://github.com/tc39/ecma402/pull/500

Object.defineProperty(Intl.NumberFormat, Symbol.hasInstance, {
    get() { throw new Error("Intl.NumberFormat[@@hasInstance] lookup"); }
});

var nf;
assertDoesNotThrow(() => nf = new Intl.NumberFormat());
assertDoesNotThrow(() => nf.format(123));
assertDoesNotThrow(() => nf.resolvedOptions());
