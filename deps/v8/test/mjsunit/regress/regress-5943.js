// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbo

function createHTML() {
  return '' + '<div><div><di';
}

createHTML();
%OptimizeFunctionOnNextCall(createHTML);

/./.test(createHTML());
