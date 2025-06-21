// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function () {
    var __v_182 = function () {
        var __v_184 = [ 3];
        var __v_186 = function () {
        };
        __v_184.map(__v_186);
    };
    %PrepareFunctionForOptimization(__v_182);
    __v_182();
    %OptimizeFunctionOnNextCall(__v_182);
    __v_182();
})();
