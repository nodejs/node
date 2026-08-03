// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

{
  let r = /\u{10000}/ugd;
  r.lastIndex = 1;
  assertArrayEquals([[0, 2]], r.exec("\u{10000}_\u{10000}").indices);
}
{
  let r = /[\u{10000}]/ugd;
  r.lastIndex = 1;
  assertArrayEquals([[0, 2]], r.exec("\u{10000}_\u{10000}").indices);
}
