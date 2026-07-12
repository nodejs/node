// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const v5 = `
  (function () {
    foo();
  })();
  {
    function foo(a11) {
        return i7;
    }
  }
`;
assertThrows(()=>(0,eval)(v5));
