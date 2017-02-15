// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var i = 0
function valueOf() {
  while (true) return i++ < 4 ? 1 + this : 2
}

1 + ({valueOf})
