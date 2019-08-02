// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// MODULE

// Flags: --allow-natives-syntax

import * as m from "modules-skip-3.js";

function get() {
  return m.stringlife;
}

%PrepareFunctionForOptimization(get);
assertEquals("42", get());
assertEquals("42", get());
assertEquals("42", get());
%OptimizeFunctionOnNextCall(get);
assertEquals("42", get());
