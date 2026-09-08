// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-lazy-feedback-allocation

class ParentData {}
ParentData.prototype.data = 42;

class ChildData extends ParentData {
  m() {
    return super.data;
  }
}

assertEquals(42, ChildData.prototype.m.call(undefined));
