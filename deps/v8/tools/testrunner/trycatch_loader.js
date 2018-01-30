// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


// Wrapper loading javascript tests passed as arguments used by gc fuzzer.
// It ignores all exceptions and run tests in a separate namespaces.
//
// It can't prevent %AbortJS function from aborting execution, so it should be
// used with d8's --disable-abortjs flag to ignore all possible errors inside
// tests.
for (let jstest of arguments) {
  print("Loading " + jstest);

  // anonymous function to not populate global namespace.
  (function () {
    let realm = Realm.create();
    try {
      Realm.eval(realm, "load(\"" + jstest + "\");");
    } catch (err) {
      // ignore all errors
    }
    Realm.dispose(realm);
  })();
}
