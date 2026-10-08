// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --shared-string-table --expose-externalize-string
// Flags: --transition-strings-during-gc-with-stack --gc-global

// An external two-byte string whose content fits in one byte. Internalization
// canonicalizes such a string to a one-byte SeqOneByteString copy, and the
// transition to a ThinString finalizes (frees) the external resource.
const raw = ("a".repeat(32) + "ሴ").slice(0, 32);
let s = createExternalizableTwoByteString(raw);
externalizeString(s);
let shared = %ShareObject(s);    // Shared, not internalized.
%ConstructInternalizedString(shared);  // Added to the StringForwardingTable.

function f(x) { return x.slice(0, 4); }

// The substring builtin scans the external resource for one-byte content, then
// allocates the result. With --gc-global and
// --transition-strings-during-gc-with-stack, that allocation internalizes {s}
// in place and frees its external resource, leaving the raw source pointer
// dangling. The copy must re-check the source and bail to the runtime instead
// of reading freed memory.
%SimulateNewspaceFull();
assertEquals("aaaa", f(shared));
