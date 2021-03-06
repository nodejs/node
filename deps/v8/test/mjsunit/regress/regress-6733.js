// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let x;
Realm.eval(Realm.current(), "let y");
assertFalse(delete x);
assertFalse(delete y);
assertFalse(eval("delete x"));
assertFalse(eval("delete y"));

(function() {
  let z;
  assertFalse(delete x);
  assertFalse(delete y);
  assertFalse(delete z);
  assertFalse(eval("delete x"));
  assertFalse(eval("delete y"));
  assertFalse(eval("delete z"));
})();

assertFalse(eval("let z; delete z"));
