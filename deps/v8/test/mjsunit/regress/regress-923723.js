// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --stack-size=100

function __f_3() {
  try {
    __f_3();
  } catch(e) {
    eval("let fun = ({a} = {a: 30}) => {");
  }
}
assertThrows(__f_3);
