// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --min-preparse-length=0

"use strict";
{
  let one = () => {
    return "example.com";
  };

  let two = () => {
    return one();
  };

  assertEquals("example.com", two());
}
