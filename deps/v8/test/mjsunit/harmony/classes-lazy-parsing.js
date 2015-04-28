// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-classes --min-preparse-length=0

'use strict';

class Base {
  m() {
    return 42;
  }
}

class Derived extends Base {
  m() {
    return super.m();
  }
}

assertEquals(42, new Derived().m());
