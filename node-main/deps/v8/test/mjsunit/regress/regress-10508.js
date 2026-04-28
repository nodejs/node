// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Error.prepareStackTrace = (error, frames) => {
  // JSON.stringify executes the replacer, triggering the relevant
  // code in Invoke().
  JSON.stringify({}, frames[0].getFunction());
};
let v0;
try {
  throw new Error();
} catch (e) {
  e.stack
}
