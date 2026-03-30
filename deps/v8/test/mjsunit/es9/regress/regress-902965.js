// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Previously, spreading in-object properties would always treat double fields
// as tagged, potentially dereferencing a Float64.
function inobjectDouble() {
  "use strict";
  this.x = -3.9;
}
const instance = new inobjectDouble();
const clone = { ...instance, };
