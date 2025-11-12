// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function driver(n) {
  let result = 0;
  for (var i=0;i<n;i++) {
    result += frozenArray.hasOwnProperty(''+i)==-1?0:1;
  }
  return result;
}

function HasOwnProperty() {
  driver(1e4);
}

function HasOwnPropertyWarmUp() {
  driver(1e1);
  driver(1e2);
  driver(1e3);
}

createSuite('HasOwnProperty', 10, HasOwnProperty, HasOwnPropertyWarmUp);
