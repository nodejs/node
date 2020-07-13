// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-for-fuzzing --fuzzing

// Test whitelisted/blacklisted intrinsics in the context of fuzzing.

// Blacklisted intrinsics are replaced with undefined.
assertEquals(undefined, %ConstructConsString("a", "b"));

// Blacklisted intrinsics can have wrong arguments.
assertEquals(undefined, %ConstructConsString(1, 2, 3, 4));

// We don't care if an intrinsic actually exists.
assertEquals(undefined, %FooBar());

// Check whitelisted intrinsic.
assertNotEquals(undefined, %IsBeingInterpreted());

// Whitelisted runtime functions with too few args are ignored.
assertEquals(undefined, %DeoptimizeFunction());

// Superfluous arguments are ignored.
%DeoptimizeFunction(function() {}, undefined);
assertNotEquals(undefined, %IsBeingInterpreted(1, 2, 3));
