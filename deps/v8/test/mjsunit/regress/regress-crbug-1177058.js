// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function __f_8() {
  Object.prototype.__defineGetter__(0, () => {
    throw Error();
  });
})();

function __f_9() {
};
assertThrows( () => { new Worker(__f_9, {
                                 type: 'function',
                                 arguments: [,]})});
