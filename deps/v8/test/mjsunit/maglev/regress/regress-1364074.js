// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --maglev

class Base {
}
let Class = class extends Base {
  constructor() {
      super();
  }
};
for (let i = 0; i < 10; i++) {
    Class = class extends Class {
      constructor() {
        try {
          super();
          super();
        } catch (e) {}
      }
    };
}
let instance = new Class();
