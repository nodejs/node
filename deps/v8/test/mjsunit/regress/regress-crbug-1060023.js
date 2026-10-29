// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class b extends RegExp {
  exec() {
    (function() { (a = (function({} = this) {})) => {} })
  }
}
assertThrows(()=>'a'.match(new b), TypeError);
