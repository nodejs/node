// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

assertTrue(%SetIteratorProtector());
assertTrue(%MapIteratorProtector());
assertTrue(%StringIteratorProtector());
assertTrue(%ArrayIteratorProtector());
var str = 'ott';
var iterator = str[Symbol.iterator]();
iterator.next = () => ({value : undefined, done : true});
assertTrue(%SetIteratorProtector());
assertTrue(%MapIteratorProtector());
assertFalse(%StringIteratorProtector());
assertTrue(%ArrayIteratorProtector());
