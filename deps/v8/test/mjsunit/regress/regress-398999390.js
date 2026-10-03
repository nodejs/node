// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --expose-externalize-string

const internalizedString = 'aaaaaaaaaaaaaaaaaaaaaaaaaaabbbbbbbbbbbbbbbbbbbbbbbbbbbbb';
const str = createExternalizableTwoByteString(internalizedString);
const [sub_str] = /b+/.exec(str);
const obj = {};
obj[str] = 'foo';
assertEquals('"bbbbbbbbbbbbbbbbbbbbbbbbbbbbb"', JSON.stringify(sub_str));
