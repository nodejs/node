// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --maglev --allow-natives-syntax

function __f_41() {
    return "abc".charCodeAt(undefined/2);
}

%PrepareFunctionForOptimization(__f_41);
assertEquals(97, __f_41());
%OptimizeMaglevOnNextCall(__f_41);
assertEquals(97, __f_41());
