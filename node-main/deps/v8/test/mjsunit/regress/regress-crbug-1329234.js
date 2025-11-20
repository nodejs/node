// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

try {
  Realm.createAllowCrossRealmAccess();
  var div = new d8.dom.Div();
  div['nodeType']();
} catch (err) {
  console.log(err);
}
