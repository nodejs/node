// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function assertAsync(b, s) {
  if (!b) {
    %AbortJS(" FAILED!")
  }
}

class P extends Promise {
  constructor() {
    super(...arguments)
    return new Proxy(this, {
      get: (_, key) => {
        return key == 'then' ?
            this.then.bind(this) :
            this.constructor.resolve(20)
      }
    })
  }
}

let p = P.resolve(10)
p.key.then(v => assertAsync(v === 20));
