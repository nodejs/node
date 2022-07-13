// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ensure `delete this` throws before `super` is called.
assertThrows(()=>{
  new class extends Object {
    constructor() {
      delete this;
      super();
    }
  }
}, ReferenceError);

// ensure `delete this` doesn't throw after `super` is called.
new class extends Object {
  constructor() {
    super();
    delete this;
  }
}
