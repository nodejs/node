// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class MyRegExp extends RegExp {
  exec(str) {
    const r = super.exec.call(this, str);
    if (r) r[0] = 0;
    return r;
  }
}

const result = 'a'.match(new MyRegExp('.', 'g'));
assertArrayEquals(result, ['0']);
