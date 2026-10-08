// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-statistics

// Regression test for issue 521921424.
// Setting a setter on Object.prototype can cause re-entrancy or other issues
// during v8::Object::Set in getV8Statistics, which used to crash with a
// fatal error because it called .FromJust() on an empty Maybe.

Object.defineProperty(Object.prototype, "total_committed_bytes", {
  set: function() {
    getV8Statistics();
  },
});
getV8Statistics();
