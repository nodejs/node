// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function $DONE(error) {
  if (error) {
    if(typeof error === 'object' && error !== null && 'name' in error) {
      print('Test262:AsyncTestFailure:' + error.name + ': ' + error.message);
    } else {
      print('Test262:AsyncTestFailure:Test262Error: ' + String(error));
    }
    quit(1);
  }

  print('Test262:AsyncTestComplete');
  quit(0);
};
