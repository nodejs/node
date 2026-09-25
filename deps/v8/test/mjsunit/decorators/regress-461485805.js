// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --js-decorators

((C = class {
   accessor p = function() {
     return 42;
   };
 }) => {
  new C();
})();
