// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --stress-lazy-source-positions

eval(`
  eval("");
  (function f() {
    // This undefined should always be known to be the global undefined value,
    // even though there is a sloppy eval call inside the top eval scope.
    return undefined;
  })();
`);

// The above logic should work through multiple layers of eval nesting.
eval(`
  eval(\`
    eval(\\\`
      eval("");
      (function f() {
        return undefined;
      })();
    \\\`);
  \`);
`);
