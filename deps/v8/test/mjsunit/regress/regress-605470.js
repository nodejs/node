// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --ignition

function function_with_m_args(m) {
  var source = '(function f() { return; })(';
  for (var arg = 0; arg < m ; arg++) {
    if (arg != 0) source += ',';
    source += arg;
  }
  source += ')';
  return eval(source);
}

function_with_m_args(0x7FFF);
