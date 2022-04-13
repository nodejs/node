// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Verify ECMA402 PR 500 Use OrdinaryHasInstance in normative optional steps
// https://github.com/tc39/ecma402/pull/500

Object.defineProperty(Intl.DateTimeFormat, Symbol.hasInstance, {
    get() { throw new Error("Intl.DateTimeFormat[@@hasInstance] lookup"); }
});

var dtf;
assertDoesNotThrow(() => dtf = new Intl.DateTimeFormat());
assertDoesNotThrow(() => dtf.format(new Date()));
assertDoesNotThrow(() => dtf.resolvedOptions());
