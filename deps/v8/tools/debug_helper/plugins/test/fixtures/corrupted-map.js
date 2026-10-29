// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class Klass {
  constructor() {
    this.a = 1;
    this.b = 2;
  }

  corrupt() {
    corruptObjectMapAndAbort(this);
  }
}

const instance = new Klass();
instance.corrupt();
