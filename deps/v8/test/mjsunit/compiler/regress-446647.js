// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --always-opt --turbo-filter=* --allow-natives-syntax

function f(a,b) {
  a%b
};

f({ toString : function() { %DeoptimizeFunction(f); }});
