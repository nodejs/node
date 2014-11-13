// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --cache=code --serialize-toplevel

var foo = [];
foo[0] = "bar";
assertEquals(["bar"], foo);

var a;
var b = 1;
a = [2];               // STORE_IC
a[0] = a[0] + 1;       // KEYED_STORE_IC, KEYED_LOAD_IC, BINARY_OP_IC
assertTrue(a[0] > b);  // CALL_IC, COMPARE_IC
b = b == null;         // COMPARE_NIL_IC
b = b || Boolean('');  // TO_BOOLEAN_IC
assertFalse(b);
