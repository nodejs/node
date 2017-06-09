// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbo-escape

function f(a) { return [a] }

assertEquals([23], f(23));
assertEquals([42], f(42));
%OptimizeFunctionOnNextCall(f);
assertEquals([65], f(65));
