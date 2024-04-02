// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-stress-opt

assertTrue(%ArraySpeciesProtector());
assertTrue(%PromiseSpeciesProtector());
assertTrue(%RegExpSpeciesProtector());
assertTrue(%TypedArraySpeciesProtector());
Object.defineProperty(Promise.prototype, "constructor", { value: {} });
assertTrue(%ArraySpeciesProtector());
assertFalse(%PromiseSpeciesProtector());
assertTrue(%RegExpSpeciesProtector());
assertTrue(%TypedArraySpeciesProtector());
