// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let r = Realm.createAllowCrossRealmAccess();
let triggerImport = Realm.eval(r, `
  function triggerImport() {
    let p = import('non_existent.js');
    p.catch(e => {});
  }
  triggerImport;
`);

Realm.navigateSameOrigin(r);
triggerImport();
