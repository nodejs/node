// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --js-nonextensible-applies-to-private

let accessObj = d8.test.createAccessCheckedObject("same-context");
Object.preventExtensions(accessObj);
let proxy = new Proxy(accessObj, {});
let nestedProxy = new Proxy(proxy, {});
Realm.shared = { proxy, nestedProxy };
let r = Realm.create();
Realm.eval(r, `
  let { proxy, nestedProxy } = Realm.shared;
  class Base { constructor(o) { return o; } }
  class SubField extends Base {
    #x = 42;
  }
  class SubMethod extends Base {
    #m() {}
  }
  new SubField(proxy);
  new SubField(nestedProxy);
  new SubMethod(proxy);
  new SubMethod(nestedProxy);
`);
