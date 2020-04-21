// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-weak-refs

// FinalizationRegistry#cleanupSome is normative optional and has its own
// flag. Test that it's not present with only --harmony-weak-refs.

assertEquals(undefined, Object.getOwnPropertyDescriptor(
  FinalizationRegistry.prototype, "cleanupSome"));
assertEquals(undefined, FinalizationRegistry.prototype.cleanupSome);
assertFalse('cleanupSome' in FinalizationRegistry.prototype);
