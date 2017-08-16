// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --ignition --ignition-osr

function osr() {
  for (var i = 0; i < 50000; ++i) Math.random();
}
osr();
