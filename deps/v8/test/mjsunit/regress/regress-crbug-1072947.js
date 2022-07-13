// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  class reg extends RegExp {}

  let r;
  function trigger() {
    try {
      trigger();
    } catch {
      Reflect.construct(RegExp,[],reg);
    }
  }
  trigger();
})();

(function() {
  class reg extends Function {}

  let r;
  function trigger() {
    try {
      trigger();
    } catch {
      Reflect.construct(RegExp,[],reg);
    }
  }
  trigger();
})();
