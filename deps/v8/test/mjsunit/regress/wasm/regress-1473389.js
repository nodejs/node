// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var memory_properties_1 = {};
Object.defineProperty(memory_properties_1, 'initial', {
  get: function() {
    throw new Error('boom: initial');
  }
});
assertThrows(
    () => new WebAssembly.Memory(memory_properties_1), Error, 'boom: initial');

var memory_properties_2 = {initial: 10};
Object.defineProperty(memory_properties_2, 'maximum', {
  get: function() {
    throw new Error('boom: maximum');
  }
});
assertThrows(
    () => new WebAssembly.Memory(memory_properties_2), Error, 'boom: maximum');
