// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --js-base-64

(function TestBase64FromToBase64WithNullOption() {
  assertThrows(() => Uint8Array.fromBase64('Zm9vYmE', null), TypeError);

  target = new Uint8Array([255, 255, 255, 255, 255]);
  assertThrows(() => target.setFromBase64('Zm9vYmE', null), TypeError);

  assertThrows(
      () => (new Uint8Array([102, 111, 111, 98, 97, 114])).toBase64(null),
      TypeError);
})();
