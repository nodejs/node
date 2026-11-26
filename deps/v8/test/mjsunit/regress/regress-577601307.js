// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-externalize-string

// 2-byte string containing only 1-byte contents.
let str = [("\u1234//" + "a".repeat(100)).slice(1), "b".repeat(100)].join("");
assertFalse(isOneByteString(str));

// evaling the string computes the hash.
eval(str);
assertFalse(isOneByteString(str));

// Internalizing the string internalizes as 1-byte.
let dummy = {};
dummy[str] = 1;
assertTrue(isOneByteString(str));

// Cons-string with left side being the same content as `str`.
let left = ["//" + "a".repeat(100), "b".repeat(100)].join("");
let cons = left + "c".repeat(20);
assertTrue(isOneByteString(cons));

// Internalizing `left` transitions it to a ThinString.
dummy[left] = 1;
assertTrue(isOneByteString(left));
assertTrue(isOneByteString(cons));

assertTrue(isOneByteString(createExternalizableString(cons)));
