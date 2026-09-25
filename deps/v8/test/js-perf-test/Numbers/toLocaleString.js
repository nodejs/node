// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function NumberToLocaleString() {
  Number(0).toLocaleString()
  Number(-12).toLocaleString()
  Number(13).toLocaleString()
  Number(123456789).toLocaleString()
  Number(1234567.89).toLocaleString()
  Number(-123456789).toLocaleString()
  Number(-1234567.89).toLocaleString()
}
createSuite('toLocaleString', 100000, NumberToLocaleString, ()=>{});
