// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.debugger.enable();

for (let i = 0; i < 20; i++) {
  let reject = function() {};
  delete reject.length;

  for (let j = 0; j < i; j++) {
    reject['p' + j] = 1;
  }

  Promise.race.call(function(executor) {
    executor(() => {}, reject);
  }, []);

  let child = Object.create(reject);
  for (let k = 0; k < 10; k++) {
    let _ = child.p0;
  }
}
