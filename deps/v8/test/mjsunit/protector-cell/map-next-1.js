// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

assertTrue(%SetIteratorProtector());
assertTrue(%MapIteratorProtector());
assertTrue(%StringIteratorProtector());
assertTrue(%ArrayIteratorProtector());
const mapIterator = new Map().values();
Object.defineProperty(mapIterator, "next", { value: {} });
assertTrue(%SetIteratorProtector());
assertFalse(%MapIteratorProtector());
assertTrue(%StringIteratorProtector());
assertTrue(%ArrayIteratorProtector());
