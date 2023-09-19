// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Error.prepareStackTrace = function(exception, frames) {
  return frames[0].getEvalOrigin();
}

try {
  Realm.eval(0, "throw new Error('boom');");
} catch(e) {
  print(e.stack);
}
