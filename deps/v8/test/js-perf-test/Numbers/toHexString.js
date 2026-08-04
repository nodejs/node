// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function NumberToHexString() {
  Number(0).toString(16);
  Number(-12).toString(16);
  Number(13).toString(16);
  Number(123456789).toString(16);
  Number(1234567.89).toString(16);
  Number(-123456789).toString(16);
  Number(-1234567.89).toString(16);
}
createSuite('toHexString', 100000, NumberToHexString, ()=>{});
