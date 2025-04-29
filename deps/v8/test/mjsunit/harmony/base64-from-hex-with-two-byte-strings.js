// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-externalize-string --js-base-64

(function TestBase64FromHexWithTwByteStrings() {
    var twoByteString = createExternalizableTwoByteString('666f6f626172');
    var arr = Uint8Array.fromHex(twoByteString);
    assertSame('102,111,111,98,97,114', arr.join(','));

    assertThrows(() => Uint8Array.fromHex('☉‿⊙'), SyntaxError);
  })();
