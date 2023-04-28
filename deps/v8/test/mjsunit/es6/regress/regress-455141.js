// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --no-lazy
"use strict";
class Base {
}
class Subclass extends Base {
  constructor() {
      this.prp1 = 3;
  }
}
function __f_1(){
}
