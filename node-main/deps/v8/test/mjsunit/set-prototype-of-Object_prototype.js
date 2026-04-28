// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function TestImmutableProtoype() {
  assertThrows(function() {
    Object.setPrototypeOf(Object.prototype, {});
  },
  TypeError,
  'Immutable prototype object \'Object.prototype\' cannot have their prototype set');
}

TestImmutableProtoype();

Object.prototype.foo = {};

TestImmutableProtoype();
