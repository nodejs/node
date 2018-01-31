// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --harmony-bigint --no-opt

const MAX_BIGINT_BITS = 1024 * 1024;  // Matches BigInt::kMaxLengthBits
const MAX_BIGINT_CHARS = MAX_BIGINT_BITS / 4;

const TOO_MANY_ONES = Array(MAX_BIGINT_CHARS + 2).join("1") + "n";

const tooBigHex = "0x" + TOO_MANY_ONES;

assertThrows(tooBigHex, SyntaxError);
