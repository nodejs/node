// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Do not crash on non-JSFunction input.
%NeverOptimizeFunction(undefined);
%NeverOptimizeFunction(true);
%NeverOptimizeFunction(1);
%NeverOptimizeFunction({});
assertThrows("%NeverOptimizeFunction()", SyntaxError);
