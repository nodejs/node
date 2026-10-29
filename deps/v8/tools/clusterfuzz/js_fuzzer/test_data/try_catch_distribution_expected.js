// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original: differential_fuzz/fake_resource.js
print("I'm a resource.");

// Original: differential_fuzz/fake_resource.js
print("I'm a resource.");

// Original: try_catch_distribution.js
try {
  baz();
} catch (e) {}
try {
  baz();
} catch (e) {}
try {
  if (foo) {
    try {
      baz();
    } catch (e) {}
    try {
      baz();
    } catch (e) {}
    try {
      if (bar) {
        try {
          baz();
        } catch (e) {}
        try {
          baz();
        } catch (e) {}
      }
    } catch (e) {}
  }
} catch (e) {}
// Original: try_catch_distribution.js
baz();
try {
  baz();
} catch (e) {}
try {
  if (foo) {
    try {
      baz();
    } catch (e) {}
    try {
      baz();
    } catch (e) {}
    try {
      if (bar) {
        baz();
        baz();
      }
    } catch (e) {}
  }
} catch (e) {}
// Original: try_catch_distribution.js
try {
  baz();
} catch (e) {}
try {
  baz();
} catch (e) {}
try {
  if (foo) {
    try {
      baz();
    } catch (e) {}
    try {
      baz();
    } catch (e) {}
    try {
      if (bar) {
        baz();
        baz();
      }
    } catch (e) {}
  }
} catch (e) {}
// Original: try_catch_distribution.js
try {
  baz();
} catch (e) {}
try {
  baz();
} catch (e) {}
try {
  if (foo) {
    try {
      baz();
    } catch (e) {}
    try {
      baz();
    } catch (e) {}
    try {
      if (bar) {
        try {
          baz();
        } catch (e) {}
        try {
          baz();
        } catch (e) {}
      }
    } catch (e) {}
  }
} catch (e) {}
// Original: try_catch_distribution.js
try {
  baz();
} catch (e) {}
try {
  baz();
} catch (e) {}
try {
  if (foo) {
    baz();
    baz();
    if (bar) {
      baz();
      baz();
    }
  }
} catch (e) {}
// Original: try_catch_distribution.js
try {
  baz();
} catch (e) {}
try {
  baz();
} catch (e) {}
try {
  if (foo) {
    baz();
    baz();
    if (bar) {
      baz();
      baz();
    }
  }
} catch (e) {}
