// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var code = "(function* gen() {"
for (var i = 0; i < 256; ++i) {
  code += `var v_${i} = 0;`
}
code += `yield; })`

var gen = eval(code);
var g = gen();
g.next();
