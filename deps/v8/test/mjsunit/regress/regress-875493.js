// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function test() {
  const re = /./y;
  re.lastIndex = 3;
  const str = 'fg';
  return re[Symbol.replace](str, '$');
}

%SetForceSlowPath(false);
const fast = test();
%SetForceSlowPath(true);
const slow = test();
%SetForceSlowPath(false);

assertEquals(slow, fast);
