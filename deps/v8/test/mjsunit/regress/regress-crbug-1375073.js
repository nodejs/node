// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --stress-flush-code --stress-marking=100 --gc-interval=411 --gc-global
// Flags: --omit-default-ctors

class B { }
class C extends B {
  #field = 'test';
}
for (let i = 0; i < 10000; i++) {
  new C();
}
try { } catch(e) { }
