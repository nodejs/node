// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class X {
  #x;
  setX(o, val) { o.#x = val; }
}

new X().setX({}, 1);
