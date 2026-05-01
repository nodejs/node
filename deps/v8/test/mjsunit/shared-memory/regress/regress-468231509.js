// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --expose-gc --expose-externalize-string --shared-string-table
// Flags: --allow-natives-syntax

function getString2() {
  return 'madness';
}
function getString() {
  return createExternalizableString('this' + 'is' + getString2());
}
function foo() {
  // Create an external shared string. This will create the first entry in the
  // string forwarding table, which will be dead by the time GC is triggered.
  let str_shared = %ShareObject(getString());
  // Trigger internalization, without creating any transitions.
  str_shared[str_shared] = true;
  externalizeString(str_shared);
}
function bar() {
  // Create a new internalized string with the same content. This will create
  // the second entry in the forwarding table, referencing the first entry as
  // forwarded string.
  let str_shared_int = %ShareObject(getString());
  str_shared_int[str_shared_int] = true;
  return str_shared_int;
}
foo();
let s = bar();
// Trigger a GC without stack.
assertPromiseResult(gc({'execution': 'async'}), function() {
  let lookup_str = 'this' + 'is' + 'madness';
  let obj = {'thisismadness': 'indeed'};
  assertEquals('indeed', obj[lookup_str]);
});
