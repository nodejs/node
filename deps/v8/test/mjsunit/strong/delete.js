// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --strong-mode

(function NoDelete() {
  const o = {a: 0};
  assertThrows("'use strong'; delete o.a", SyntaxError);
  assertThrows("'use strong'; delete o", SyntaxError);
})();
