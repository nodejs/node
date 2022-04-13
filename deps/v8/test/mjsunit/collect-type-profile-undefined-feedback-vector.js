// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f(a, b, c) {
  return 'bye';
};

%CollectTypeProfile(0, f, undefined);
