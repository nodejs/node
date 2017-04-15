// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --ignition-staging --turbo --validate-asm --always-opt

function Module() {
  "use asm";
  return {};
}
var m = Module();
