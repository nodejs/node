// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --shared-string-table
// Flags: --transition-strings-during-gc-with-stack --gc-global

// A two-byte sequential string with one-byte-only content. Sharing flattens
// the slice into a shared SeqTwoByteString.
const content = ("a".repeat(16) + "\u1234").slice(0, 16);
let a = %ShareObject(content);    // Shared, not internalized.
%ConstructInternalizedString(a);  // Added to the StringForwardingTable.

function f(x) { return x.slice(0, 4); }
%PrepareFunctionForOptimization(f);
%OptimizeFunctionOnNextCall(f);

// With --gc-global and --transition-strings-during-gc-with-stack, the
// allocation inside the substring builtin turns {a} into a ThinString while
// the source is being copied. The copy must re-check the representation and
// bail out instead of reading the ThinString's fields as characters.
%SimulateNewspaceFull();
assertEquals("aaaa", f(a));
