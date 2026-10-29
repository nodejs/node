// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


// Wrapper loading javascript tests passed as arguments used by gc fuzzer.
// It ignores all exceptions and run tests in a separate namespaces.
//
// It can't prevent %AbortJS function from aborting execution, so it should be
// used with d8's --disable-abortjs flag to ignore all possible errors inside
// tests.

// We use -- as an additional separator for test preamble files and test files.
// The preamble files (before --) will be loaded in each realm before each
// test.
var separator = arguments.indexOf("--")
var preamble = arguments.slice(0, separator)
var tests = arguments.slice(separator + 1)

var preambleString = ""
for (let jstest of preamble) {
  preambleString += "d8.file.execute(\"" + jstest + "\");"
}

for (let jstest of tests) {
  print("Loading " + jstest);
  let start = performance.now();

  // anonymous function to not populate global namespace.
  (function () {
    let realm = Realm.create();
    try {
      Realm.eval(realm, preambleString + "d8.file.execute(\"" + jstest + "\");");
    } catch (err) {
      // ignore all errors
    }
    Realm.dispose(realm);
  })();

  let durationSec = ((performance.now() - start) / 1000.0).toFixed(2);
  print("Duration " + durationSec + "s");
}
