// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc

// Register an object in a FinalizationRegistry with a Symbol as the unregister
// token.
let fr = new FinalizationRegistry(function () {});
(function register() {
  fr.register({}, "holdings", Symbol('unregisterToken'));
})();
// The unregister token should be dead, trigger its collection.
gc();
