// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute("test/mjsunit/mjsunit.js");

let reject_handler;
let promise = new Promise(function(resolve, reject) {
  reject_handler = reject;
});

assertPromiseResult(promise);

reject_handler("Reject with callback");
