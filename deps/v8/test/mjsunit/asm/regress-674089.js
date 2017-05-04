// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --validate-asm --ignition --lazy-inner-functions

function outer() {
  "use asm";
  function inner() {
    switch (1) {
      case 0:
        break foo;
    }
  }
}
outer();
