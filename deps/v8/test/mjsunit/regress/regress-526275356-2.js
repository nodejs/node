// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --shared-string-table
// Flags: --transition-strings-during-gc-with-stack --gc-global

// A two-byte sequential string with one-byte-only content: U+212A (Kelvin
// sign) lowercases to 'k' but keeps the two-byte representation, so this is
// "ak" as a SeqTwoByteString.
const a = %ShareObject("A\u212A".toLowerCase());
%ConstructInternalizedString(a);  // Added to the StringForwardingTable.

function f(x, y) { return x + y; }
%PrepareFunctionForOptimization(f);
f(a, "b\u1234");
f(a, "b\u1234");
%OptimizeFunctionOnNextCall(f);

// The result allocation inside the string-add builtin turns {a} into a
// ThinString mid-concatenation; the copy must re-check the representation and
// bail to the runtime instead of reading the ThinString's fields as
// characters.
%SimulateNewspaceFull();
assertEquals("akb\u1234", f(a, "b\u1234"));
