// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

d8.file.execute('test/mjsunit/test-async.js');

testAsync(assert => {
  assert.plan(1);
  const error = new TypeError('Throwing');
  Promise.resolve({ then(resolve, reject) {
    throw error;
  }}).then(v => {
    assert.unreachable();
  }, e => {
    assert.equals(error, e);
  });
});
